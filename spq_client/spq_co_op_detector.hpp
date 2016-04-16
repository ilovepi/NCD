/*
  The MIT License

  Copyright (c) 2015-2016 Paul Kirth pk1574@gmail.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/**
 * @author: Paul Kirth
 * @file: ip_checksum.h
 */

#ifndef SPQ_CO_OP_DETECTOR_HPP
#define SPQ_CO_OP_DETECTOR_HPP

#include "co_op_udp_detector.hpp"

namespace detection
{
    class spq_co_op_detector : public co_op_udp_detector
    {
    public:
        enum priority{ high, low};


        spq_co_op_detector(priority priority_in, uint16_t saturation_port_in, uint16_t  saturate_length, int test_id_in, const std::string &dest_ip, uint8_t tos, uint16_t id, uint16_t frag_off,
                           uint8_t ttl, uint8_t proto, uint16_t check_sum, uint32_t sport, uint32_t dport, bool last,
                           const std::string &filename, uint16_t num_packets, uint16_t data_length, uint16_t num_tail,
                           uint16_t tail_wait, raw_level raw_status, transport_type trans_proto) : co_op_udp_detector(
                test_id_in, dest_ip, tos, id, frag_off, ttl, proto, check_sum, sport, dport, last, filename,
                num_packets, data_length, num_tail, tail_wait, raw_status, trans_proto), trafficPriority(priority_in), saturation_port(saturation_port_in), saturate_train_length(saturate_length)
        {
            setup_packet_train();
        }

        virtual ~spq_co_op_detector()
        {
        }

        virtual void setup_packet_train() override
        {
            saturation_train.reserve(saturate_train_length);

            for(int i = 0; i < num_packets; ++i)
                saturation_train.push_back(std::make_shared<packet>(payload_size));
            uint16_t packet_id = 0;
            for(auto& item : saturation_train)
            {
                item->fill(file, htons(packet_id++));
            }

            detector::setup_packet_train();
        }

        virtual void send_train() override
        {
            send_saturation_packets();
            base_udp_detector::send_train();
        }

        virtual void send_saturation_packets()
        {
            int saturation_fd             = socket(res->ai_family, SOCK_DGRAM, IPPROTO_UDP);


            if(saturation_fd == -1)
            {
                perror("call to socket() failed for SEND");
                exit(EXIT_FAILURE);
            }        // end error check

            if(res->ai_family != AF_INET)
            {
                errno = EAFNOSUPPORT;
                perror("ncd only supports IPV4 at this time");
                exit(EXIT_FAILURE);
            }        // end error check

            // set TTL
            setsockopt(send_fd, IPPROTO_IP, IP_TTL, &ip_header.ttl, sizeof(ip_header.ttl));
            socklen_t size = 1500U * num_packets;
            setsockopt(send_fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
            int n;
            /*send data train*/
            for(const auto& item : saturation_train)
            {
                n = (int)sendto(send_fd, item->data.data(), item->data.size(), 0, res->ai_addr, res->ai_addrlen);
                if(n == -1)
                {
                    perror("call to sendto() failed: error sending UDP udp train");
                    exit(EXIT_FAILURE);
                }        // end if
            }            // end for
        }


    private:
        priority trafficPriority;
        uint16_t saturation_port;
        uint16_t saturate_train_length;
        packet_buffer_t saturation_train;
        int saturation_fd;
    };

}// end namespace

#endif //SPQ_CO_OP_DETECTOR_HPP