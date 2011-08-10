// copyright 2011 t. schneider tes@mit.edu
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.


// tests functionality of std::list<const google::protobuf::Message*> calls

#include "goby/acomms/dccl.h"
#include "goby/acomms/libdccl/dccl_field_codec_default.h"
#include "test.pb.h"
#include "goby/util/as.h"
#include "goby/util/time.h"
#include "goby/util/binary.h"

using goby::acomms::operator<<;

int main(int argc, char* argv[])
{
    goby::glog.add_stream(goby::util::Logger::DEBUG3, &std::cerr);
    goby::glog.set_name(argv[0]);

    goby::acomms::DCCLCodec* codec = goby::acomms::DCCLCodec::get();

    GobyMessage1 msg_in1;
    GobyMessage2 msg_in2;
    GobyMessage3 msg_in3;
    GobyMessage3 msg_in4;

    msg_in1.set_int32_val(1);
    msg_in2.set_bool_val(false);
    msg_in3.set_string_val("string1");
    msg_in4.set_string_val("string2");
    
    std::list<const google::protobuf::Message*> msgs;
    msgs.push_back(&msg_in1);
    msgs.push_back(&msg_in2);
    msgs.push_back(&msg_in3);    
    msgs.push_back(&msg_in4);

    std::list<const google::protobuf::Descriptor*> descs;
    descs.push_back(msg_in1.GetDescriptor());
    descs.push_back(msg_in2.GetDescriptor());
    descs.push_back(msg_in3.GetDescriptor());    
    descs.push_back(msg_in4.GetDescriptor());
    
    codec->info_repeated(descs, &std::cout);    

    BOOST_FOREACH(const google::protobuf::Message* p, msgs)
    {
        static int i = 0;
        std::cout << "Message " << ++i << " in:\n" << p->DebugString() << std::endl;
    }
    
    assert(codec->validate_repeated(descs));

    std::cout << "Try encode..." << std::endl;
    std::string bytes1 = codec->encode_repeated(msgs) + std::string(4, '\0');
    std::cout << "... got bytes (hex): " << goby::util::hex_encode(bytes1) << std::endl;
    std::cout << "Try decode..." << std::endl;
    
    std::list< boost::shared_ptr<google::protobuf::Message> > msgs_out = codec->decode_repeated(bytes1);

    std::list<const google::protobuf::Message*>::const_iterator in_it = msgs.begin();

    assert(msgs.size() == msgs_out.size());
    
    BOOST_FOREACH(boost::shared_ptr<google::protobuf::Message> p, msgs_out)
    {
        static int i = 0;
        std::cout << "... got Message " << ++i << " out:\n" << p->DebugString() << std::endl;
        assert((*in_it)->SerializeAsString() == p->SerializeAsString());
        ++in_it;
    }
    

    std::cout << "all tests passed" << std::endl;
}

