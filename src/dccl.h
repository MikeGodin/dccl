// Copyright 2009-2012 Toby Schneider (https://launchpad.net/~tes)
//                     Massachusetts Institute of Technology (2007-)
//                     Woods Hole Oceanographic Institution (2007-)
//                     Goby Developers Team (https://launchpad.net/~goby-dev)
// 
//
// This file is part of the Goby Underwater Autonomy Project Libraries
// ("The Goby Libraries").
//
// The Goby Libraries are free software: you can redistribute them and/or modify
// them under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The Goby Libraries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.


#ifndef DCCL20091211H
#define DCCL20091211H

#include <string>
#include <set>
#include <map>
#include <ostream>
#include <stdexcept>
#include <vector>

#include <google/protobuf/descriptor.h>

#include <boost/shared_ptr.hpp>

#include "goby/common/time.h"
#include "goby/common/logger.h"
#include "dccl/protobuf/dccl.pb.h"
#include "goby/acomms/acomms_helpers.h"
#include "goby/util/binary.h"
#include "goby/util/dynamic_protobuf_manager.h"

#include "protobuf_cpp_type_helpers.h"
#include "dccl_exception.h"
#include "dccl_field_codec.h"
#include "dccl_field_codec_fixed.h"
#include "dccl_field_codec_default.h"
#include "dccl_type_helper.h"
#include "dccl_field_codec_manager.h"

/// The global namespace for the Goby project.
namespace goby
{
    namespace util
    { class FlexOstream; }
 
 
    /// Objects pertaining to acoustic communications (acomms)
    namespace acomms
    {
        class DCCLFieldCodec;
  
        /// \class DCCLCodec dccl/dccl.h dccl/dccl.h
        /// \brief provides an API to the Dynamic CCL Codec.
        /// \ingroup acomms_api
        /// \ingroup dccl_api
        /// \sa acomms_dccl.proto and acomms_modem_message.proto for definition of Google Protocol Buffers messages (namespace goby::acomms::protobuf).
        ///
        /// Simple usage example:
        /// 1. Define a Google Protobuf message with DCCL extensions:
        /// \verbinclude simple.proto
        /// 2. Write a bit of code like this:
        /// \code
        /// goby::acomms::DCCLCodec* dccl = goby::acomms::DCCLCodec::get();
        /// dccl->validate<Simple>();
        /// Simple message_out;
        /// message_out.set_telegram("Hello!");
        /// std::string bytes;
        /// dccl->encode(&bytes, message);
        /// \\ send bytes across some network
        /// Simple message_in;
        /// dccl->decode(bytes&, message_in);
        /// \endcode
        /// \example acomms/chat/chat.cpp
        /// \example acomms/dccl/dccl_simple/dccl_simple.cpp
        /// simple.proto
        /// \verbinclude simple.proto
        /// dccl_simple.cpp
        /// \example acomms/dccl/two_message/two_message.cpp
        /// two_message.proto
        /// \verbinclude two_message.proto
        /// two_message.cpp
        class DCCLCodec
        {
          public:
            DCCLCodec();
            virtual ~DCCLCodec() { }

            static const std::string DEFAULT_CODEC_NAME;

            /// \name Initialization Methods.
            // NOTE: uncommented next two lines from doxygen because of bug in v1.7.4 and newer: https://bugzilla.gnome.org/show_bug.cgi?id=660501 
            // These methods are intended to be called before doing any work with the class. However,
            // they may be called at any time as desired.
            //@{

            /// \brief Set (and overwrite completely if present) the current configuration. (protobuf::DCCLConfig defined in acomms_dccl.proto)
            void set_cfg(const protobuf::DCCLConfig& cfg);
            
            /// \brief Set (and merge "repeated" fields) the current configuration. (protobuf::DCCLConfig defined in acomms_dccl.proto). Non-repeated fields will be overwritten if set.
            void merge_cfg(const protobuf::DCCLConfig& cfg);

            /// \brief Load any codecs present in the given shared library handle
            ///
            /// Codecs must be loaded within the shared library using a C function
            /// (declared extern "C") called "goby_dccl_load" with the signature
            /// void goby_dccl_load(goby::acomms::DCCLCodec* codec)
            void load_shared_library_codecs(void* dl_handle);
            
            /// \brief All messages must be validated (size checks, option extensions checks, etc.) before they can be encoded/decoded. Use this form when the messages used are static (known at compile time).
            ///
            /// \tparam ProtobufMessage Any Google Protobuf Message generated by protoc (i.e. subclass of google::protobuf::Message)
            /// \throw DCCLException if message is invalid. Warnings and errors are written to goby::glog.
            template<typename ProtobufMessage>
                void validate()
            { validate(ProtobufMessage::descriptor()); }

            //@}            


            
            /// \name Informational Methods.
            ///
            /// Provides various forms of information about the DCCLCodec
            //@{

            /// \brief Writes a human readable summary (including field sizes) of the provided DCCL type to the stream provided.
            ///
            /// \tparam ProtobufMessage Any Google Protobuf Message generated by protoc (i.e. subclass of google::protobuf::Message)
            /// \param os Pointer to a stream to write this information
            template<typename ProtobufMessage>
                void info(std::ostream* os) const
            { info(ProtobufMessage::descriptor(), os); }

            /// \brief Writes a human readable summary (including field sizes) of all the loaded (validated) DCCL types
            ///
            /// \param os Pointer to a stream to write this information            
            void info_all(std::ostream* os) const;
            
            /// \brief Gives the DCCL id (defined by the custom message option extension "(goby.msg).dccl.id" in the .proto file). This ID is used on the wire to unique identify incoming message types.
            ///
            /// \tparam ProtobufMessage Any Google Protobuf Message generated by protoc (i.e. subclass of google::protobuf::Message)
            template <typename ProtobufMessage>
                unsigned id() const
            { return id(ProtobufMessage::descriptor()); }            
            
            /// \brief Provides the encoded size (in bytes) of msg. This is useful if you need to know the size of a message before encoding it (encoding it is generally much more expensive than calling this method)
            ///
            /// \param msg Google Protobuf message with DCCL extensions for which the encoded size is requested
            /// \return Encoded (using DCCL) size in bytes
            unsigned size(const google::protobuf::Message& msg);

            /// \brief The group name for goby::glog that all encoding related messages are written.
            static const std::string& glog_encode_group() { return glog_encode_group_; }
            /// \brief The group name for goby::glog that all decoding related messages are written.
            static const std::string& glog_decode_group() { return glog_decode_group_; }
            
            //@}
       
            /// \name Codec functions.
            ///
            /// This is where the real work happens.
            //@{

            /// \brief Encodes a DCCL message
            ///
            /// \param bytes Pointer to byte string to store encoded msg
            /// \param msg Message to encode (must already have been validated)
            /// \throw DCCLException if message cannot be encoded.
            void encode(std::string* bytes, const google::protobuf::Message& msg);
            
            /// \brief Decode a DCCL message when the type is known at compile time.
            ///
            /// \param bytes encoded message to decode (must already have been validated)
            /// \param msg Pointer to any Google Protobuf Message generated by protoc (i.e. subclass of google::protobuf::Message). The decoded message will be written here.
            /// \throw DCCLException if message cannot be decoded.
            void decode(const std::string& bytes, google::protobuf::Message* msg);

            /// \brief Get the DCCL ID of an unknown encoded DCCL message.
            /// 
            /// You can use this method along with id() to handle multiple types of known (static) incoming DCCL messages. For example:
            /// \code
            /// unsigned dccl_id = codec->id_from_encoded(bytes);    
            /// if(dccl_id == codec->id<MyProtobufType1>())
            /// {
            ///     MyProtobufType1 msg_out1;
            ///     codec->decode(bytes, &msg_out1);
            /// }
            /// else if(dccl_id == codec->id<MyProtobufType2>())
            /// {
            ///     MyProtobufType2 msg_out2;
            ///     codec->decode(bytes, &msg_out2);
            /// }
            /// \endcode
            /// \param bytes encoded message to get the DCCL ID of
            /// \return DCCL ID
            /// \sa test/acomms/dccl8/test.cpp and test/acomms/dccl8/test.proto
            unsigned id_from_encoded(const std::string& bytes);
            

            //@}

            /// \name Alternative Dynamic Protobuf methods (including list of Message methods) (Advanced)
            ///
            /// These methods are intended to be used when the Google Protobuf message types are not known at compile-time, and work with the <a href="http://code.google.com/apis/protocolbuffers/docs/reference/cpp/google.protobuf.descriptor.html">Descriptor</a>/<a href="http://code.google.com/apis/protocolbuffers/docs/reference/cpp/google.protobuf.message.html#Reflection">Reflection</a> meta-data introspection API provided by Google.
            //@{

            
            /// \brief An alterative form for validating messages for message types <i>not</i> known at compile-time ("dynamic").
            ///
            /// \param desc The Google Protobuf "Descriptor" (meta-data) of the message to validate.
            /// \throw DCCLException if message is invalid.
            void validate(const google::protobuf::Descriptor* desc);

            /// \brief Shortcut for validating multiple messages at once
            ///
            /// \throw DCCLException if message(s) are invalid.
            /// \param descs list of Google Protobuf "Descriptors" to validate
            /// \sa test/acomms/dccl4/test.cpp and test/acomms/dccl4/test.proto
            void validate_repeated(const std::list<const google::protobuf::Descriptor*>& descs);
            
            /// \brief An alterative form for getting information for messages for message types <i>not</i> known at compile-time ("dynamic").
            void info(const google::protobuf::Descriptor* desc, std::ostream* os) const;

            /// \brief Shortcut for getting information on multiple messages at once
            void info_repeated(const std::list<const google::protobuf::Descriptor*>& desc, std::ostream* os) const;

            /// \brief Provides the DCCL ID given a DCCL type.
            unsigned id(const google::protobuf::Descriptor* desc) const {
                return desc->options().GetExtension(goby::msg).dccl().id();
            }
            
            /// \brief Returns the encoded size of multiple messages at once (sum of all sizes). You must use this method (instead of N calls to size, where N is msgs.size()) to get the size of messages encoded using encode_repeated as repeated encoding may be more efficient (in terms of the encoded message size) than N calls of encode
            ///
            /// \tparam GoogleProtobufMessagePointer anything that acts like a pointer (has operator*) to a google::protobuf::Message (smart pointers like boost::shared_ptr included)
            /// \param msgs list of DCCL messages to get the sizes of
            /// \return total size of the encoded messages in bytes
            template<typename GoogleProtobufMessagePointer>
                unsigned size_repeated(const std::list<GoogleProtobufMessagePointer>& msgs)
            {
                unsigned out = 0;
                BOOST_FOREACH(const GoogleProtobufMessagePointer& msg, msgs)
                    out += size(*msg);
                return out;
            }

            /// \brief An alterative form for decoding messages for message types <i>not</i> known at compile-time ("dynamic").
            ///
            /// \tparam GoogleProtobufMessagePointer anything that acts like a pointer (has operator*) to a google::protobuf::Message (smart pointers like boost::shared_ptr included)
            /// \param bytes the byte string returned by encode
            /// \throw DCCLException if message cannot be decoded
            /// \return pointer to decoded message (a google::protobuf::Message). You are responsible for deleting the memory used by this pointer. This message can be examined using the Google Reflection/Descriptor API.
            template<typename GoogleProtobufMessagePointer>
                GoogleProtobufMessagePointer decode(const std::string& bytes)
                {
                    // ownership of this object goes to the caller of decode()
                    unsigned id = id_from_encoded(bytes);   

                    if(!id2desc_.count(id))
                        throw(DCCLException("Message id " + goby::util::as<std::string>(id) + " has not been validated. Call validate() before decoding this type."));
                    
                    GoogleProtobufMessagePointer msg =
                        goby::util::DynamicProtobufManager::new_protobuf_message<GoogleProtobufMessagePointer>(id2desc_.find(id)->second);
                    decode(bytes, &(*msg));
                    return msg;
                }
            
            
            /// \brief Encode multiple messages at once. In general this is more efficient (i.e. produces smaller messages) than calling encode repeatedly.
            ///
            /// \tparam GoogleProtobufMessagePointer anything that acts like a pointer (has operator*) to a google::protobuf::Message (smart pointers like boost::shared_ptr included)
            /// \param msgs list of DCCL messages to encode
            /// \throw DCCLException if message(s) cannot be encoded.
            /// \return string containing bytes that represent the encoded message
            /// \sa test/acomms/dccl4/test.cpp and test/acomms/dccl4/test.proto
            template<typename GoogleProtobufMessagePointer>
                std::string encode_repeated(const std::list<GoogleProtobufMessagePointer>& msgs)
            {
                std::string out;
                BOOST_FOREACH(const GoogleProtobufMessagePointer& msg, msgs)
                {
                    std::string piece;
                    encode(&piece, *msg);
                    out += piece;
                }
    
                return out;
            }
            
            /// \brief Decode multiple messages at once. Messages encoded using encode_repeated must be decoded using decode_repeated.
            ///
            /// \param orig_bytes string of bytes produced by encoded_repeated
            /// \tparam GoogleProtobufMessagePointer anything that acts like a pointer (has operator*) to a google::protobuf::Message (smart pointers like boost::shared_ptr included)
            /// \throw DCCLException if message(s) cannot be decoded.
            /// \return list of pointers to the decoded messages: you are responsible for deleting these pointers when done with them.
            /// \sa test/acomms/dccl4/test.cpp and test/acomms/dccl4/test.proto
            template<typename GoogleProtobufMessagePointer>
                std::list<GoogleProtobufMessagePointer> decode_repeated(const std::string& orig_bytes)
                {
                    std::string bytes = orig_bytes;
                    std::list<GoogleProtobufMessagePointer> out;
                    while(!bytes.empty())
                    {
                        try
                        {
                            out.push_back(decode<GoogleProtobufMessagePointer>(bytes));
                            unsigned last_size = size(*out.back());
                            glog.is(common::logger::DEBUG1) && glog  << "last message size was: " << last_size << std::endl;
                            bytes.erase(0, last_size);
                        }
                        catch(DCCLException& e)
                        {
                            if(out.empty())
                                throw(e);
                            else
                            {
                                glog.is(common::logger::WARN) && glog << "failed to decode " << goby::util::hex_encode(bytes) << " but returning parts already decoded"  << std::endl;
                                return out;
                            }
                        }        
                    }
                    return out;

                }

            
            
            //@}


            /// \name Hook API (Advanced)
            ///
            /// Register hooks (callbacks) to be called when a certain Google Protobuf option extension is encountered while exploring the Google Protobuf descriptor (message meta-data). This is used by libqueue to identify its option extensions (queue.is_src, queue.is_dest, etc.). You may be able to find another use for it as well!

            //@{
            /// \brief run hooks previously registered to DCCLFieldCodec::register_wire_value_hook
            ///
            /// All callbacks registered using DCCLFieldCodec::register_wire_value_hook() will be called when the registered custom FieldOptions extension is set. For example:
            /// \code
            /// extend .google.protobuf.FieldOptions
            /// {
            ///   optional double my_extension = 50000;
            /// }
            ///
            /// message Foo
            /// {
            ///    int32 bar = 1 [(my_extension)=123.456];
            ///    FooBar baz = 2;
            /// }
            /// \endcode
            /// Now in C++, if I write
            /// \code
            /// int main()
            ///
            /// void set_latest_metadata(const boost::any& field_value,
            ///             const boost::any& wire_value,
            ///             const boost::any& extension_value)
            /// {
            ///    assert(field_value.type() == typeid(int32));
            ///    assert(type_value.type() == typeid(int32));
            ///    assert(extension_value.type() == typeid(double));
            ///    assert(boost::any_cast<double>(extension_value) == 123.456);
            /// }
            /// 
            /// int main()
            /// {
            ///   goby::acomms::DCCLFieldCodecBase::register_wire_value_hook(
            ///                   my_extension.number(), set_latest_metadata);
            ///   Foo foo;
            ///   goby::acomms::DCCLCodec::get()->run_hooks(foo);
            ///
            /// }
            ///
            /// \endcode
            /// set_latest_metadata will be called with "int32 bar = 1" is encountered because "my_extension is set.
            void run_hooks(const google::protobuf::Message& msg);
            //@}           
            
            /// \name Custom DCCL ID Codecs (Advanced)
            ///
            /// Change the underlying DCCL ID codec to optimize your network. Be very careful you know how to identify all your messages though!

            //@{
            /// \brief Adds a DCCL id codec to be used along with a string identifier
            ///
            /// \tparam DCCLTypedFieldCodecUint32 Subclass of DCCLTypedFieldCodec<uint32> > that implements encoding / decoding of DCCL IDs
            /// \param identifier a name to give this DCCL ID codec for later use when setting it.
            template<typename DCCLTypedFieldCodecUint32>
                void add_id_codec(const std::string& identifier)
            {
                if(!id_codec_.count(identifier))
                    id_codec_[identifier] = boost::shared_ptr<DCCLTypedFieldCodec<uint32> > (new DCCLTypedFieldCodecUint32);
            }

            /// \brief Sets the DCCL id codec currently in use
            ///
            /// The DCCL ID codec will be set and used for all relevant calls to encode, decode, etc. until this method is called again with a different identifier or reset_id_codec is called.
            /// \param identifier name of this special DCCL ID codec that matches the identifier used when registering with add_id_codec().
            void set_id_codec(const std::string& identifier)
            { current_id_codec_ = identifier; }

            /// \brief Resets the DCCL id codec currently in use to the default
            void reset_id_codec()
            { set_id_codec(DEFAULT_CODEC_NAME); }
            
            //@}           

            

          private:

            // so we can use shared_ptr to hold the singleton
            template<typename T>
                friend void boost::checked_delete(T*);
            
            DCCLCodec(const DCCLCodec&);
            DCCLCodec& operator= (const DCCLCodec&);
            
            void encrypt(std::string* s, const std::string& nonce);
            void decrypt(std::string* s, const std::string& nonce);
            void process_cfg();
            
            void set_default_codecs();            
            
          private:
            static boost::shared_ptr<DCCLCodec> inst_;

            static std::string glog_encode_group_;
            static std::string glog_decode_group_;
            
            protobuf::DCCLConfig cfg_;
            // SHA256 hash of the crypto passphrase
            std::string crypto_key_;
            
            // maps `dccl.id`s onto Message Descriptors
            std::map<int32, const google::protobuf::Descriptor*> id2desc_;

            std::string current_id_codec_;
            std::map<std::string, boost::shared_ptr<DCCLTypedFieldCodec<uint32> > > id_codec_;
        };

        inline std::ostream& operator<<(std::ostream& os, const DCCLCodec& codec)
        {
            codec.info_all(&os);
            return os;
        }
    }
}

#endif
