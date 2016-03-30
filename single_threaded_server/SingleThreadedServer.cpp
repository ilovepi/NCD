//
// Created by atlas on 3/29/16.
//

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <boost/dynamic_bitset.hpp>
#include <chrono>
#include <fcntl.h>
#include <signal.h>
#include "SingleThreadedServer.hpp"

using namespace detection;

SingleThreadedServer::SingleThreadedServer()
{

    send_complete_fd[2] = {};

     capture_fd[2]={};
}

void SingleThreadedServer::run()
{
    acceptor();

}

void SingleThreadedServer::acceptor()
{
    //pid_t fork_id;
    uint16_t port = 15555;
    // start listening on a specific , non-reserved port
    listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listen_fd < 0) {
        std::ios_base::failure e("Failed to acquire socket for listen()");
        throw e;
    }
    open = true;

    int val;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // setup struct for server address
    sockaddr_in serv_addr = {};
    sockaddr_in client_addr = {};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    int err = bind(listen_fd, (sockaddr *) &serv_addr, sizeof(serv_addr));

    if(err < 0) {
        std::ios_base::failure e("Failed to bind socket for listen()");
        throw e;
    }

    listen(listen_fd, 4);
    socklen_t client_len = 0;

//    while(true)
    {
        int temp_fd = accept(listen_fd, (sockaddr *) &client_addr, &client_len);
//        fork_id = fork();
//        if(fork_id == 0)
        {
            test_results results = receive_tcp_parameters(temp_fd);
            //receive_train((detection::test_params()));
            send_tcp_reply(temp_fd, results);
        }
        close(temp_fd);

    }
    close(listen_fd);
    open = false;

}


void SingleThreadedServer::receive_train(test_params params, sockaddr_in client)
{
    char buff[1500];
    test_results results = {};
    results.success = false;


    // setup connection params
    sockaddr_in serv_addr = {};

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(params.port);

    int udp_fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
    if(udp_fd < 0) {
        terminate("failed to acquire socket for UDP data train");
    }
    std::cout << "UDP port number: " << params.port << std::endl;
    std::cout << "Packets to expect: " << params.num_packets << std::endl;
    int err = bind(udp_fd, (sockaddr *) &serv_addr, sizeof(serv_addr));

    if(err < 0) {
        terminate("Failed to bind socket for data train");
    }

    socklen_t client_len = sizeof(client);

    // recv data
    uint32_t packets_received = 0;
    boost::dynamic_bitset<> bitset(params.num_packets);
    uint16_t *id = reinterpret_cast<uint16_t *>(buff + params.offset);
    int n;
    bool send_complete = false;

    auto marker = std::chrono::high_resolution_clock::now();
    while(!send_complete && packets_received < params.num_packets) {
        n = read(send_complete_fd[1], &send_complete, sizeof(send_complete));
        if(n < 0)
        {
            if(errno != EAGAIN || errno != EWOULDBLOCK)
            {
                terminate("Error reading send complete from pipe!");
                write(send_complete_fd[1], &results, sizeof(results));
                return;
            }
        }

        if(!send_complete) {
            n = recvfrom(udp_fd, buff, sizeof(buff), 0, reinterpret_cast<sockaddr *>(&client), &client_len);

            if(n < 0) {
                if(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
                    continue;
                std::cerr << "recvfrom() in data train failed" << std::endl;
                continue;
            }
            else {
                packets_received++;
                bitset.set(ntohs(*id));
            }
        }


    }

    auto timestamp = std::chrono::high_resolution_clock::now() - marker;
    results.elapsed_time    = std::chrono::duration_cast<std::chrono::nanoseconds>(timestamp).count() / 1000000.0;

    close(udp_fd);

    // log all missing packets
    std::string packet_state;
    to_string(bitset, packet_state);

    results.lostpackets = params.num_packets - packets_received;
    std::cout << "Packets recived: " << packets_received << std::endl;
    std::cout << "Exiting Meausre()" << std::endl;

    write(send_complete_fd[1], &results, sizeof(results));
    _exit(0);

}

test_results SingleThreadedServer::receive_tcp_parameters(int sock_fd)
{
    test_results t = {};
    t.success = false;
    pipe2(send_complete_fd, O_NONBLOCK);
    // zero initialize tcp message buffer;
    char buff[1500] = {};

    //test_params* params = reinterpret_cast<test_params*>(buff);
    test_params params;

    // get experiment parameters from client
    long err = recv(sock_fd, &buff, sizeof(buff), 0);
    if(err < 0) {
        terminate("Failure receiving experimental parameters from client");
        return t;
    }

    if(err >= sizeof(test_params)) {
        params.deserialize(buff);
    }
    else {
        terminate("Failure receiving test parameters!!");
        return t;
    }

    pid_t capture_id = fork();
    if(capture_id == 0)
    {
        capture_packets(params);
        _exit(0);
    }

    pid_t fork_id = fork();
    if(fork_id == 0) {
        //child logic;
        // do the udp train.
        receive_train(params, (sockaddr_in()));
        // maybe read from pope to synchonize....
        return t;
    }
    else {
        //parent logic;
        // do the tcp_recieve;
        bool stop;
        err = recv(sock_fd, &stop, sizeof(stop), 0);
        if(err < 0) {
            terminate("Failure receiving stop signal from client");
            return t;
        }
        write(send_complete_fd[0], &stop, sizeof(stop));
        int n;
        bool send_tcp = false;
        do {
            n = read(send_complete_fd[0], &t, sizeof(t));
            if(n < 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                terminate("Error reading test results from pipe");
                return t;
            }
            else
            {
                send_tcp = true;
            }
        } while(!send_tcp);

        t.pcap_id = get_pcap_id(params.test_id);
        std::stringstream ss;
        ss << t.pcap_id <<".pcap";
        rename("temp.pcap", ss.str().c_str());

        return t;
    }


    //experiment exp(*params, client);


    //bool send_complete = false;


}

void SingleThreadedServer::terminate(std::string msg)
{

}

void SingleThreadedServer::send_tcp_reply(int sock_fd, test_results results)
{

}

void SingleThreadedServer::capture_packets(test_params params)
{
    // spawn child process -- tcpdump
        pid_t tcpdump_id = fork();

        std::ostringstream convert;
        convert << params.port;
        std::string port = convert.str();
        // std::stringstream str;
        // str << " -i eth0 "<< " src ip " << inet_ntoa(client.sin_addr) <<" and (udp dest port " << port << " and src
        // port " << client.sin_port << ")";
        if(tcpdump_id == 0)
        {
            execl("/usr/sbin/tcpdump", "/usr/sbin/tcpdump", "-i", "lo", "udp and port ", port.data(), "-w", "temp.pcap",
                  (char*)0);
            _exit(0);
        }

        // std::cout << "Waiting to kill tcpdump..." << std::endl;
        // wait to be signaled
        bool stop = false;
    int n;
    do {
        read(capture_fd[1], &stop, sizeof(stop));
        if(n < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            terminate("error encountered reading terminate signal for packet capture from pipe");
        }
    }while(!stop);
sleep(1);
        // then kill child process -- tcpdump
        kill(tcpdump_id, SIGINT);
    _exit(0);
        // std::cout << "killed tcpdump" << std::endl;


}
















