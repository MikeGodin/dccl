// copyright 2009-2011 t. schneider tes@mit.edu
// 
// this file is part of the Dynamic Compact Control Language (DCCL),
// the goby-acomms codec. goby-acomms is a collection of libraries 
// for acoustic underwater networking
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

#ifndef DCCLTypeHelper20110405H
#define DCCLTypeHelper20110405H

#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/not.hpp>
#include <boost/mpl/logical.hpp>

#include "protobuf_cpp_type_helpers.h"

namespace goby
{
    namespace acomms
    {

        class DCCLTypeHelper
        {
          public:
            /* static const boost::shared_ptr<FromProtoTypeBase> find(google::protobuf::FieldDescriptor::Type type); */
                
            /* static const boost::shared_ptr<FromProtoCppTypeBase> find(const google::protobuf::FieldDescriptor* field) */
            /* { */
            /*     if(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) */
            /*         return find(field->message_type()); */
            /*     else */
            /*         return find(field->cpp_type()); */
   
            /* } */
                
            /* static const boost::shared_ptr<FromProtoCppTypeBase> find(const google::protobuf::Descriptor* desc) */
            /* { */
            /*     return find(google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE, */
            /*                 desc->full_name()); */
            /* } */
            
            static boost::shared_ptr<FromProtoTypeBase> find(google::protobuf::FieldDescriptor::Type type);
            static boost::shared_ptr<FromProtoCppTypeBase> find(const google::protobuf::FieldDescriptor* field)
            {
                if(field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE)
                    return find(field->message_type());
                else
                    return find(field->cpp_type());
            }
                
            static boost::shared_ptr<FromProtoCppTypeBase> find(const google::protobuf::Descriptor* desc)
            {
                return find(google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE,
                            desc->full_name());
            }
                
            /* static const boost::shared_ptr<goby::acomms::FromProtoCppTypeBase> find(google::protobuf::FieldDescriptor::CppType cpptype, const std::string& type_name = "") const; */
            static boost::shared_ptr<goby::acomms::FromProtoCppTypeBase> find(google::protobuf::FieldDescriptor::CppType cpptype, const std::string& type_name = "");

                
            template<typename ProtobufMessage>
                static void add()
            {
                custom_message_map_.insert(std::make_pair(ProtobufMessage::descriptor()->full_name(), boost::shared_ptr<FromProtoCppTypeBase>(new FromProtoCustomMessage<ProtobufMessage>)));
            }

          private:
            DCCLTypeHelper() { initialize(); }            
            ~DCCLTypeHelper() { }
            DCCLTypeHelper(const DCCLTypeHelper&);
            DCCLTypeHelper& operator= (const DCCLTypeHelper&);
            void initialize();    
            
          private:
            // so we can use shared_ptr to hold the singleton
            template<typename T>
                friend void boost::checked_delete(T*);
            static boost::shared_ptr<DCCLTypeHelper> inst_;
            
            typedef std::map<google::protobuf::FieldDescriptor::Type,
                boost::shared_ptr<FromProtoTypeBase> > TypeMap;
            static TypeMap type_map_;

            typedef std::map<google::protobuf::FieldDescriptor::CppType,
                boost::shared_ptr<FromProtoCppTypeBase> > CppTypeMap;
            static CppTypeMap cpptype_map_;

            typedef std::map<std::string,
                boost::shared_ptr<FromProtoCppTypeBase> > CustomMessageMap;
            static CustomMessageMap custom_message_map_;
        };                      /*  */
    }
}
#endif