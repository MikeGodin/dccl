// t. schneider tes@mit.edu 11.20.09
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

#include <iostream>

#include "goby/common/logger.h"
#include "goby/moos/moos_translator.h"
#include "test.pb.h"
#include "basic_node_report.pb.h"
#include "goby/util/binary.h"


using namespace goby::moos;

void populate_test_msg(TestMsg* msg_in);
void run_one_in_one_out_test(MOOSTranslator& translator, int i, bool hex_encode);


int main(int argc, char* argv[])
{
    goby::glog.add_stream(goby::common::logger::DEBUG3, &std::cout);
    goby::glog.set_name(argv[0]);

    
    protobuf::TranslatorEntry entry;
    entry.set_protobuf_name("TestMsg");    

    protobuf::TranslatorEntry::CreateParser* parser = entry.add_create();
    parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_PROTOBUF_TEXT_FORMAT);
    parser->set_moos_var("TEST_MSG_1");

    protobuf::TranslatorEntry::PublishSerializer* serializer = entry.add_publish();
    serializer->set_technique(protobuf::TranslatorEntry::TECHNIQUE_PROTOBUF_TEXT_FORMAT);
    serializer->set_moos_var("TEST_MSG_1");

    const double LAT_ORIGIN = 42.5;
    const double LON_ORIGIN = 10.8;
    
    MOOSTranslator translator(entry, LAT_ORIGIN, LON_ORIGIN, TRANSLATOR_TEST_DIR "/modemidlookup.txt");

    CMOOSGeodesy geodesy;
    geodesy.Initialise(LAT_ORIGIN, LON_ORIGIN);
    
    
    goby::glog << translator << std::endl;    
    run_one_in_one_out_test(translator, 0, false);
    
    

    std::set<protobuf::TranslatorEntry> entries;
    {                
        protobuf::TranslatorEntry entry;
        entry.set_protobuf_name("TestMsg");

        protobuf::TranslatorEntry::CreateParser* parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_PROTOBUF_NATIVE_ENCODED);
        parser->set_moos_var("TEST_MSG_1");

        protobuf::TranslatorEntry::PublishSerializer* serializer = entry.add_publish();
        serializer->set_technique(protobuf::TranslatorEntry::TECHNIQUE_PROTOBUF_NATIVE_ENCODED);
        serializer->set_moos_var("TEST_MSG_1");

        entries.insert(entry);
    }
    
    translator.add_entry(entries);

    goby::glog << translator << std::endl;    
    run_one_in_one_out_test(translator, 1, true);


    {
        protobuf::TranslatorEntry entry;
        entry.set_protobuf_name("TestMsg");
        
        protobuf::TranslatorEntry::CreateParser* parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_COMMA_SEPARATED_KEY_EQUALS_VALUE_PAIRS);
        parser->set_moos_var("TEST_MSG_1");
        
        protobuf::TranslatorEntry::PublishSerializer* serializer = entry.add_publish();
        serializer->set_technique(protobuf::TranslatorEntry::TECHNIQUE_COMMA_SEPARATED_KEY_EQUALS_VALUE_PAIRS);
        serializer->set_moos_var("TEST_MSG_1");
        
        translator.add_entry(entry);
    }
    
    goby::glog << translator << std::endl;    
    run_one_in_one_out_test(translator, 2, false);



    
    std::string format_str = "NAME=%1%,X=%202%,Y=%3%,HEADING=%201%,REPEAT={%10%}";
   {
        protobuf::TranslatorEntry entry;
        entry.set_protobuf_name("BasicNodeReport");
        
        protobuf::TranslatorEntry::CreateParser* parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        parser->set_moos_var("NODE_REPORT");
        parser->set_format(format_str);
        
        protobuf::TranslatorEntry::PublishSerializer* serializer = entry.add_publish();
        serializer->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        serializer->set_moos_var("NODE_REPORT");
        serializer->set_format(format_str);
        
        translator.add_entry(entry);
    }

    goby::glog << translator << std::endl;

    BasicNodeReport report;
    report.set_name("unicorn");
    report.set_x(550);
    report.set_y(1023.5);
    report.set_heading(240);
    report.add_repeat(1);
    report.add_repeat(-1);
    
    
    std::multimap<std::string, CMOOSMsg> moos_msgs = translator.protobuf_to_moos(report);    

    for(std::multimap<std::string, CMOOSMsg>::const_iterator it = moos_msgs.begin(),
            n = moos_msgs.end();
        it != n;
        ++ it)
    {
        goby::glog << "Variable: " << it->first << "\n"
                   << "Value: " << it->second.GetString() << std::endl;
        assert(it->second.GetString() == "NAME=unicorn,X=550,Y=1023.5,HEADING=240,REPEAT={1,-1}");
    }
    
    typedef std::auto_ptr<google::protobuf::Message> GoogleProtobufMessagePointer;
    GoogleProtobufMessagePointer report_out =
        translator.moos_to_protobuf<GoogleProtobufMessagePointer>(moos_msgs, "BasicNodeReport");

    goby::glog << "Message out: " << std::endl;
    goby::glog << report_out->DebugString() << std::endl;    
    assert(report_out->SerializeAsString() == report.SerializeAsString());
    

   {
        protobuf::TranslatorEntry entry;
        entry.set_protobuf_name("BasicNodeReport");
        
        protobuf::TranslatorEntry::CreateParser* parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        parser->set_moos_var("NAV_X");
        parser->set_format("%202%");

        parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        parser->set_moos_var("VEHICLE_NAME");
        protobuf::TranslatorEntry::CreateParser::Algorithm* algo_in = parser->add_algorithm();
        algo_in->set_name("to_lower");
        algo_in->set_primary_field(1);
        parser->set_format("%1%");

        parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_COMMA_SEPARATED_KEY_EQUALS_VALUE_PAIRS);
        parser->set_moos_var("NAV_HEADING");
        algo_in = parser->add_algorithm();
        algo_in->set_name("angle_0_360");
        algo_in->set_primary_field(201);
        

        parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        parser->set_moos_var("NAV_Y");
        parser->set_format("%3%");

        protobuf::TranslatorEntry::PublishSerializer* serializer = entry.add_publish();
        serializer->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        serializer->set_moos_var("NODE_REPORT_FORMAT");
        serializer->set_format(format_str + ";LAT=%100%;LON=%101%");
        
        protobuf::TranslatorEntry::PublishSerializer::Algorithm* algo_out = serializer->add_algorithm();
        algo_out->set_name("utm_x2lon");
        algo_out->set_output_virtual_field(101);
        algo_out->set_primary_field(202);
        algo_out->add_reference_field(3);

        algo_out = serializer->add_algorithm();
        algo_out->set_name("utm_y2lat");
        algo_out->set_output_virtual_field(100);
        algo_out->set_primary_field(3);
        algo_out->add_reference_field(202);

        algo_out = serializer->add_algorithm();
        algo_out->set_name("name2modem_id");
        algo_out->set_output_virtual_field(102);
        algo_out->set_primary_field(1);

        algo_out = serializer->add_algorithm();
        algo_out->set_name("name2modem_id");
        algo_out->set_output_virtual_field(103);
        algo_out->set_primary_field(1);
        
        algo_out = serializer->add_algorithm();
        algo_out->set_name("modem_id2type");
        algo_out->set_output_virtual_field(103);
        algo_out->set_primary_field(1);
        
        algo_out = serializer->add_algorithm();
        algo_out->set_name("to_upper");
        algo_out->set_output_virtual_field(103);
        algo_out->set_primary_field(1);
        
        
        protobuf::TranslatorEntry::PublishSerializer* serializer2 = entry.add_publish();
        serializer2->CopyFrom(*serializer);
        serializer2->clear_format();
        serializer2->set_technique(protobuf::TranslatorEntry::TECHNIQUE_COMMA_SEPARATED_KEY_EQUALS_VALUE_PAIRS);
        serializer2->set_moos_var("NODE_REPORT_KEY_VALUE");
        
        translator.add_entry(entry);
   }
    
   goby::glog << translator << std::endl;
    
   moos_msgs.clear();
   moos_msgs.insert(std::make_pair("NAV_X", CMOOSMsg(MOOS_NOTIFY, "NAV_X", report.x())));
   moos_msgs.insert(std::make_pair("NAV_Y", CMOOSMsg(MOOS_NOTIFY, "NAV_Y", report.y())));
   moos_msgs.insert(std::make_pair("NAV_HEADING", CMOOSMsg(MOOS_NOTIFY, "NAV_HEADING", "heading=-120")));
   moos_msgs.insert(std::make_pair("VEHICLE_NAME", CMOOSMsg(MOOS_NOTIFY, "VEHICLE_NAME", "UNICORN")));

   report_out =
       translator.moos_to_protobuf<GoogleProtobufMessagePointer>(moos_msgs, "BasicNodeReport");

   report.clear_repeat();
   
   goby::glog << "Message in: " << std::endl;
   goby::glog << report.DebugString() << std::endl;    
   goby::glog << "Message out: " << std::endl;
   goby::glog << report_out->DebugString() << std::endl;    

   assert(report_out->SerializeAsString() == report.SerializeAsString());


   moos_msgs = translator.protobuf_to_moos(*report_out);
    
   for(std::multimap<std::string, CMOOSMsg>::const_iterator it = moos_msgs.begin(),
           n = moos_msgs.end();
       it != n;
       ++ it)
   {
       goby::glog << "Variable: " << it->first << "\n"
                  << "Value: " << it->second.GetString() << std::endl;

       if(it->first == "NODE_REPORT_FORMAT")
           assert(it->second.GetString() == "NAME=unicorn,X=550,Y=1023.5,HEADING=240,REPEAT={};LAT=42.5091075598637;LON=10.806955912844");
       else if(it->first == "NODE_REPORT_KEY_VALUE")
           assert(it->second.GetString() == "name=unicorn,x=550,y=1023.5,heading=240,utm_y2lat(y)=42.5091075598637,utm_x2lon(x)=10.806955912844,name2modem_id(name)=3,name2modem_id+modem_id2type+to_upper(name)=AUV");
       
   }
    
   

   std::string sub_message_format_str = "em.val=%17:1%";
   {
        protobuf::TranslatorEntry entry;
        entry.set_protobuf_name("TestMsg");
        
        protobuf::TranslatorEntry::CreateParser* parser = entry.add_create();
        parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        parser->set_moos_var("TEST_MSG_1");
        parser->set_format(sub_message_format_str);
        
        protobuf::TranslatorEntry::PublishSerializer* serializer = entry.add_publish();
        serializer->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
        serializer->set_moos_var("TEST_MSG_1");
        serializer->set_format(sub_message_format_str);
        
        translator.add_entry(entry);
    }

    goby::glog << translator << std::endl;

    TestMsg embedded_test;
    embedded_test.mutable_msg_default_optional()->set_val(19.998);    
    moos_msgs = translator.protobuf_to_moos(embedded_test);  
    
    for(std::multimap<std::string, CMOOSMsg>::const_iterator it = moos_msgs.begin(),
            n = moos_msgs.end();
        it != n;
        ++ it)
    {
        goby::glog << "Variable: " << it->first << "\n"
                   << "Value: " << it->second.GetString() << std::endl;
        assert(it->second.GetString() == "em.val=19.998");
    }
    
    typedef std::auto_ptr<google::protobuf::Message> GoogleProtobufMessagePointer;
    GoogleProtobufMessagePointer embedded_test_out =
        translator.moos_to_protobuf<GoogleProtobufMessagePointer>(moos_msgs, "TestMsg");

    goby::glog << "Message out: " << std::endl;
    goby::glog << embedded_test_out->DebugString() << std::endl;    
    assert(embedded_test_out->SerializePartialAsString() == embedded_test.SerializePartialAsString());


    
   sub_message_format_str = "em0.val=%117.0:1%,1uint64=%106.1%,0uint64=%106.0%.2uint64=%106.2%:em1.val=%117.1:1%,dbl0=%101.0%,dbl1=%101.1%,dbl2=%101.2%,dbl3=%101.3%";
   {
       protobuf::TranslatorEntry entry;
       entry.set_protobuf_name("TestMsg");
       
       protobuf::TranslatorEntry::CreateParser* parser = entry.add_create();
       parser->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
       parser->set_moos_var("TEST_MSG_1");
       parser->set_format(sub_message_format_str);
       
       protobuf::TranslatorEntry::PublishSerializer* serializer = entry.add_publish();
       serializer->set_technique(protobuf::TranslatorEntry::TECHNIQUE_FORMAT);
       serializer->set_moos_var("TEST_MSG_1");
       serializer->set_format(sub_message_format_str);
       
       translator.add_entry(entry);
   }
   
   goby::glog << translator << std::endl;
   
   embedded_test.Clear();
   embedded_test.add_msg_default_repeat()->set_val(21.123); 
   embedded_test.add_msg_default_repeat()->set_val(100.5);
   embedded_test.add_uint64_default_repeat(0);
   embedded_test.add_uint64_default_repeat(100);
   embedded_test.add_uint64_default_repeat(200);
   moos_msgs = translator.protobuf_to_moos(embedded_test);  
    
   for(std::multimap<std::string, CMOOSMsg>::const_iterator it = moos_msgs.begin(),
           n = moos_msgs.end();
       it != n;
       ++ it)
   {
       goby::glog << "Variable: " << it->first << "\n"
                  << "Value: " << it->second.GetString() << std::endl;
       assert(it->second.GetString() == "em0.val=21.123,1uint64=100,0uint64=0.2uint64=200:em1.val=100.5,dbl0=0,dbl1=0,dbl2=0,dbl3=0");
   }
   
   typedef std::auto_ptr<google::protobuf::Message> GoogleProtobufMessagePointer;
   embedded_test_out = translator.moos_to_protobuf<GoogleProtobufMessagePointer>(moos_msgs, "TestMsg");

    goby::glog << "Message out: " << std::endl;
    goby::glog << embedded_test_out->DebugString() << std::endl;    

    embedded_test.add_double_default_repeat(0);
    embedded_test.add_double_default_repeat(0);
    embedded_test.add_double_default_repeat(0);
    embedded_test.add_double_default_repeat(0);
    assert(embedded_test_out->SerializePartialAsString() == embedded_test.SerializePartialAsString());

    
    
    std::cout << "all tests passed" << std::endl;
}

void run_one_in_one_out_test(MOOSTranslator& translator, int i, bool hex_encode)
{
    TestMsg msg;
    populate_test_msg(&msg);
    
    std::multimap<std::string, CMOOSMsg> moos_msgs = translator.protobuf_to_moos(msg);    

    for(std::multimap<std::string, CMOOSMsg>::const_iterator it = moos_msgs.begin(),
            n = moos_msgs.end();
        it != n;
        ++ it)
    {
        goby::glog << "Variable: " << it->first << "\n"
                   << "Value: " << (hex_encode ? goby::util::hex_encode(it->second.GetString()) : it->second.GetString()) << std::endl;

        switch(i)
        {
            case 0:
            {    
                std::string test;
                goby::moos::MOOSTranslation<protobuf::TranslatorEntry::TECHNIQUE_PROTOBUF_TEXT_FORMAT>::serialize(&test, msg);;
                
                assert(it->second.GetString() == test);
                assert(it->first == "TEST_MSG_1");
                break;
            }
            
            case 1:
            {
                assert(it->second.GetString() == msg.SerializeAsString());
                assert(it->first == "TEST_MSG_1");
                break;
            }

            case 2:
            {
                std::string test;
                goby::moos::MOOSTranslation<protobuf::TranslatorEntry::TECHNIQUE_COMMA_SEPARATED_KEY_EQUALS_VALUE_PAIRS>::serialize(&test, msg, google::protobuf::RepeatedPtrField<protobuf::TranslatorEntry::PublishSerializer::Algorithm>());
                assert(it->second.GetString() == test);
                assert(it->first == "TEST_MSG_1");
                break;
            }
            
            
            default:
                assert(false);
        };
        
        
        ++i;
    }
    
    typedef std::auto_ptr<google::protobuf::Message> GoogleProtobufMessagePointer;
    GoogleProtobufMessagePointer msg_out =
        translator.moos_to_protobuf<GoogleProtobufMessagePointer>(moos_msgs, "TestMsg");

    goby::glog << "Message out: " << std::endl;
    goby::glog << msg_out->DebugString() << std::endl;    
    assert(msg_out->SerializeAsString() == msg.SerializeAsString());
}


void populate_test_msg(TestMsg* msg_in)
{
    int i = 0;
    msg_in->set_double_default_optional(++i + 0.1);
    msg_in->set_float_default_optional(++i + 0.2);

    msg_in->set_int32_default_optional(++i);
    msg_in->set_int64_default_optional(-++i);
    msg_in->set_uint32_default_optional(++i);
    msg_in->set_uint64_default_optional(++i);
    msg_in->set_sint32_default_optional(-++i);
    msg_in->set_sint64_default_optional(++i);
    msg_in->set_fixed32_default_optional(++i);
    msg_in->set_fixed64_default_optional(++i);
    msg_in->set_sfixed32_default_optional(++i);
    msg_in->set_sfixed64_default_optional(-++i);

    msg_in->set_bool_default_optional(true);

    msg_in->set_string_default_optional("abc123");
    msg_in->set_bytes_default_optional(goby::util::hex_decode("00112233aabbcc1234"));
    
    msg_in->set_enum_default_optional(ENUM_C);
    msg_in->mutable_msg_default_optional()->set_val(++i + 0.3);
    msg_in->mutable_msg_default_optional()->mutable_msg()->set_val(++i);

    msg_in->set_double_default_required(++i + 0.1);
    msg_in->set_float_default_required(++i + 0.2);

    msg_in->set_int32_default_required(++i);
    msg_in->set_int64_default_required(-++i);
    msg_in->set_uint32_default_required(++i);
    msg_in->set_uint64_default_required(++i);
    msg_in->set_sint32_default_required(-++i);
    msg_in->set_sint64_default_required(++i);
    msg_in->set_fixed32_default_required(++i);
    msg_in->set_fixed64_default_required(++i);
    msg_in->set_sfixed32_default_required(++i);
    msg_in->set_sfixed64_default_required(-++i);

    msg_in->set_bool_default_required(true);

    msg_in->set_string_default_required("abc123");
    msg_in->set_bytes_default_required(goby::util::hex_decode("00112233aabbcc1234"));
    
    msg_in->set_enum_default_required(ENUM_C);
    msg_in->mutable_msg_default_required()->set_val(++i + 0.3);
    msg_in->mutable_msg_default_required()->mutable_msg()->set_val(++i);

    
    for(int j = 0; j < 2; ++j)
    {
        msg_in->add_double_default_repeat(++i + 0.1);
        msg_in->add_float_default_repeat(++i + 0.2);
        
        msg_in->add_int32_default_repeat(++i);
        msg_in->add_int64_default_repeat(-++i);
        msg_in->add_uint32_default_repeat(++i);
        msg_in->add_uint64_default_repeat(++i);
        msg_in->add_sint32_default_repeat(-++i);
        msg_in->add_sint64_default_repeat(++i);
        msg_in->add_fixed32_default_repeat(++i);
        msg_in->add_fixed64_default_repeat(++i);
        msg_in->add_sfixed32_default_repeat(++i);
        msg_in->add_sfixed64_default_repeat(-++i);
        
        msg_in->add_bool_default_repeat(true);
        
        msg_in->add_string_default_repeat("abc123");

        if(j)
            msg_in->add_bytes_default_repeat(goby::util::hex_decode("00aabbcc"));
        else
            msg_in->add_bytes_default_repeat(goby::util::hex_decode("ffeedd12"));
        
        msg_in->add_enum_default_repeat(static_cast<Enum1>((++i % 3) + 1));
        EmbeddedMsg1* em_msg = msg_in->add_msg_default_repeat();
        em_msg->set_val(++i + 0.3);
        em_msg->mutable_msg()->set_val(++i);
    }
}

