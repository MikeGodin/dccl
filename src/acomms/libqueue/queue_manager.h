// copyright 2009 t. schneider tes@mit.edu 
//
// this file is part of the Queue Library (libqueue),
// the goby-acomms message queue manager. goby-acomms is a collection of 
// libraries for acoustic underwater networking
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

#ifndef QueueManager20091204_H
#define QueueManager20091204_H

#include <limits>
#include <set>

#include "goby/acomms/xml/xml_parser.h"
#include "goby/acomms/dccl.h"

#include "queue_config.h"
#include "queue_key.h"
#include "queue.h"

namespace goby
{
    namespace util
    {
        class FlexOstream;
    }

    namespace acomms
    {
        /// \name Queue Library callback function type definitions
        //@{

        /// \brief boost::function for a function taking a single ModemMessage reference.
        ///
        /// Think of this as a generalized version of a function pointer (void (*)(QueueKey, ModemMessage&)). See http://www.boost.org/doc/libs/1_34_0/doc/html/function.html for more on boost:function. 
        typedef boost::function<void (QueueKey key, const ModemMessage& message)> QueueMsgFunc1;
        /// \brief boost::function for a function taking a ModemMessage reference as input and filling a ModemMessage reference as output.
        ///
        /// Think of this as a generalized version of a function pointer (void (*)(QueueKey, ModemMessage&, ModemMessage&)). See http://www.boost.org/doc/libs/1_34_0/doc/html/function.html for more on boost:function.
        typedef boost::function<bool (QueueKey key, const ModemMessage& message1, ModemMessage & message2)> QueueMsgFunc2;
        /// \brief boost::function for a function returning a single unsigned 
        ///
        /// Think of this as a generalized version of a function pointer (void (*)(QueueKey, unsigned)). See http://www.boost.org/doc/libs/1_34_0/doc/html/function.html for more on boost:function.
        typedef boost::function<void (QueueKey key, unsigned size)> QueueSizeFunc;
    
        //@}
    
        /// provides an API to the goby-acomms Queuing Library.
        class QueueManager
        {
          public:
            /// \name Constructors/Destructor
            //@{         
            /// \brief Default constructor.
            ///
            /// \param os std::ostream object or FlexOstream to capture all humanly readable runtime and debug information (optional).
            QueueManager(std::ostream* os = 0);

            /// \brief Instantiate with a single XML file.
            ///
            /// \param file path to an XML queuing configuration (i.e. contains a \verbatim <queuing/> \endverbatim section) file to parse for use by the QueueManager.
            /// \param schema path (absolute or relative to the XML file path) for the validating schema (message_schema.xsd) (optional).
            /// \param os std::ostream object or FlexOstream to capture all humanly readable runtime and debug information (optional).
            QueueManager(const std::string& file, const std::string schema = "", std::ostream* os = 0);
        
            /// \brief Instantiate with a set of XML files
            /// 
            /// \param files set of paths to XML queuing configuration (i.e. contains a \verbatim <queuing/> \endverbatim section) files to parse for use by the QueueManager.
            /// \param os std::ostream object or FlexOstream to capture all humanly readable runtime and debug information (optional).
            QueueManager(const std::set<std::string>& files, const std::string schema = "", std::ostream* os = 0);


            /// \brief Instantiate with a single QueueConfig object.
            ///
            /// \param cfg QueueConfig object to initialize a new %queue with. Use the QueueConfig largely for non-DCCL messages. Use the XML file constructors for XML configured DCCL messages.
            /// \param os std::ostream object or FlexOstream to capture all humanly readable runtime and debug information (optional).
            QueueManager(const QueueConfig& cfg, std::ostream* os = 0);
        
            /// \brief Instantiate with a set of QueueConfig objects.
            //
            /// \param cfgs Set of QueueConfig objects to initialize new %queues with. Use the QueueConfig largely for non-DCCL messages. Use the XML file constructors for XML configured DCCL messages.
            /// \param os std::ostream object or FlexOstream to capture all humanly readable runtime and debug information (optional).
            QueueManager(const std::set<QueueConfig>& cfgs, std::ostream* os = 0);

            /// Destructor.
            ~QueueManager() { }
        
            //@}
        
            /// \name Initialization Methods
            ///
            /// These methods are intended to be called before doing any work with the class. However,
            /// they may be called at any time as desired.
            //@{

            /// \brief Add more %queues by configuration XML files (typically contained in DCCL message XML files).
            ///
            /// \param xml_file path to the XML file to parse and add to this codec.
            /// \param xml_schema path to the message_schema.xsd file to validate XML with. if using a relative path this
            /// must be relative to the directory of the xml_file, not the present working directory. if not provided
            /// no validation is done.
            void add_xml_queue_file(const std::string& xml_file, const std::string xml_schema = "");

            /// \brief Add more Queues.
            ///
            /// \param QueueConfig& cfg: configuration object for the new %queue.
            void add_queue(const QueueConfig& cfg);
        

            /// \brief Set the schema used for XML syntax checking.
            /// 
            /// Schema location is relative to the XML file location!
            /// if you have XML files in different places you must pass the
            /// proper relative path (or just use absolute paths)
            /// \param schema location of the message_schema.xsd file.
            void set_schema(const std::string schema) { xml_schema_ = schema; }

            /// \brief Set the %modem id for this vehicle.
            ///
            /// \param modem_id unique (within a network) number representing the %modem on this vehicle.
            void set_modem_id(unsigned modem_id) { modem_id_ = modem_id; }

            /// \brief Set a %queue to call the data_on_demand callback every time data is request (basically forwards the %modem data_request).
            ///
            /// \param key QueueKey that references the %queue for which to enable the <tt>on demand</tt>  callback.
            void set_on_demand(QueueKey key);
            /// \brief Set a %queue to call the data_on_demand callback every time data is request (basically forwards the %modem data_request).
            /// 
            /// \param id DCCL message id (\verbatim <id/> \endverbatim) that references the %queue for which to enable the <tt>on demand</tt> callback.
            void set_on_demand(unsigned id, QueueType type = queue_dccl);

            void add_flex_groups(util::FlexOstream& tout);
            
            
            //@}

            /// \name Application level Push/Receive Methods
            ///
            /// These methods are the primary higher level interface to the QueueManager. From here you can push messages
            /// and set the callbacks to use on received messages.
            //@{

            /// \brief Push a message using a QueueKey as a key.
            ///
            /// \param key QueueKey that references the %queue to push the message to.
            /// \param new_message ModemMessage to push.
            void push_message(QueueKey key, ModemMessage& new_message);
        
            /// \brief Push a message using the %queue id and type together as a key.
            ///
            /// \param id DCCL message id (\verbatim <id/> \endverbatim) that references the %queue for which to push the message to.
            /// \param new_message ModemMessage to push.
            void push_message(unsigned id, ModemMessage& new_message, QueueType type = queue_dccl);

            /// \brief Set the callback to receive incoming DCCL messages. Any messages received before this callback is set will be discarded.
            ///
            /// \param func Pointer to function (or any other object boost::function accepts) matching the signature of queue::QueueMsgFunc1.
            /// The callback (func) will be invoked with the following parameters:
            /// \param key the QueueKey for the %queue for which this received message belongs.
            /// \param message A ModemMessage reference containing the contents of the received DCCL message.
            void set_receive_cb(QueueMsgFunc1 func)     { callback_receive = func; }

            /// \brief Set the callback to receive incoming CCL messages. Any messages received before this callback is set will be discarded.
            ///
            /// \param func Pointer to function (or any other object boost::function accepts) matching the signature of queue::QueueMsgFunc1.
            /// The callback (func) will be invoked with the following parameters:
            /// \param key the QueueKey for the %queue for which this received message belongs.
            /// \param message A ModemMessage reference containing the contents of the received CCL message.
            void set_receive_ccl_cb(QueueMsgFunc1 func) { callback_receive_ccl = func; }

            //@}
        
            /// \name Modem Driver level Push/Receive Methods
            ///
            /// These methods are the interface to the QueueManager from the %modem driver.
            //@{

            /// \brief Finds data to send to the %modem.
            /// 
            /// Data from the highest priority %queue(s) will be combined to form a message equal or less than the size requested in ModemMessage message_in. If using one of the classes inheriting ModemDriverBase, this method should be bound and passed to ModemDriverBase::set_datarequest_cb.
            /// \param message_in The ModemMessage containing the details of the request (source, destination, size, etc.)
            /// \param message_out The packed ModemMessage ready for sending by the modem. This will be populated by this function.
            /// \return true if successful in finding data to send, false if no data is available
            bool provide_outgoing_modem_data(const ModemMessage& message_in, ModemMessage& message_out);

            /// \brief Receive incoming data from the %modem.
            ///
            /// If using one of the classes inheriting ModemDriverBase, this method should be bound and passed to ModemDriverBase::set_receive_cb.
            /// \param message The received ModemMessage.
            void receive_incoming_modem_data(const ModemMessage& message);
        
            /// \brief Receive acknowledgements from the %modem.
            ///
            /// If using one of the classes inheriting ModemDriverBase, this method should be bound and passed to ModemDriverBase::set_ack_cb.
            /// \param message The ModemMessage corresponding to the acknowledgement (dest, src, frame#)
            void handle_modem_ack(const ModemMessage& message);


        
            //@}


            /// \name Active methods
            ///
            /// Call these methods when you want the QueueManager to perform time sensitive tasks
            //@{
            void do_work();

            //@}
        
            /// \name Additional Application level methods
            ///
            /// Use these methods for advanced features and more fine grained control.
            //@{
        
        
            /// \brief Set the callback to receive acknowledgements of message receipt. 
            ///
            /// \param func Pointer to function (or any other object boost::function accepts) matching the signature of queue::QueueMsgFunc1.
            /// The callback (func) will be invoked with the following parameters:
            /// \param key the QueueKey for this acknowledgement.
            /// \param message A ModemMessage containing the contents of the original message that was acknowledged. 
            void set_ack_cb(QueueMsgFunc1 func)     { callback_ack = func; }
        
            /// \brief Set the callback to process %queues marked \p on_demand by QueueManager::set_on_demand. 
            ///
            /// This method forwards a data request from the %modem to the application level for applications demanding to wait on encoding until the moment data is required (highly time sensitive data). Note that in most circumstances it is better to fill the %queues asynchronously and let the QueueManager take care of this transaction.
            /// \param func Pointer to function (or other boost::function object) of the signature queue::QueueMsgFunc2. The callback (func) will be invoked with the following parameters:
            /// \param key the QueueKey for the data request.
            /// \param message1 (incoming) The ModemMessage containing the details of the request (source, destination, size, etc.)
            /// \param message2 (outgoing) The ModemMessage to be sent. This should be populated by the callback.
            void set_data_on_demand_cb(QueueMsgFunc2 func) { callback_ondemand = func; }        
            /// \brief Set the callback to receive messages every time a %queue changes size (that is, a message is popped or pushed).
            ///
            /// \param func Pointer to function (or any other object boost::function accepts) matching the signature of queue::QueueSizeFunc.
            /// The callback (func) will be provided with the following parameters:
            /// \param key the QueueKey for %queue that changed size.
            /// \param size the size of the %queue after the size change event.
            void set_queue_size_change_cb(QueueSizeFunc func) { callback_qsize = func; }

            /// \brief Set the callback to receive messages every time a message is expired due to exceeding its time to live (ttl)
            ///
            /// \param func Pointer to function (or any other object boost::function accepts) matching the signature of queue::QueueMsgFunc1.
            /// The callback (func) will be invoked with the following parameters:
            /// \param key the QueueKey for the queue from which the expired message originated.
            /// \param message A ModemMessage containing the contents of the original message that was expired. 
            void set_expire_cb(QueueMsgFunc1 func) { callback_expire = func; }

        
            //@}

            /// \name Medium Access Control methods
            ///
            /// 
            //@{        

            /// \brief Request the %modem id of the next destination. Required by the MACManager to determine the next communication destination
            ///
            /// \param size size (in bytes) of the next transmission. Size affects the next destination since messages that are too large are disregarded.
            /// \return id of the next destination
            int request_next_destination(unsigned size = std::numeric_limits<unsigned>::max());

            //@}
        
            enum { NO_AVAILABLE_DESTINATION = -1 };
            enum { DCCL_NUM_HEADER_NIBS = DCCL_NUM_HEADER_BYTES*NIBS_IN_BYTE };
        
        
        
            /// \name Informational Methods
            ///
            //@{        

            /// \return human readable summary of all loaded %queues
            std::string summary() const;

            
            
            //@}
        
            /// \example libqueue/examples/queue_simple/queue_simple.cpp
            /// simple.xml
            /// \verbinclude queue_simple/simple.xml
            /// queue_simple.cpp


            /// \example acomms/examples/chat/chat.cpp
        
        
          private:
            void qsize(Queue* q)
            {
                if(callback_qsize)
                    callback_qsize(QueueKey(q->cfg().type(), q->cfg().id()), q->size());
            }
        
            // finds the %queue with the highest priority
            Queue* find_next_sender(ModemMessage& message, unsigned user_frame_num);
        
            // combine multiple "user" frames into a single "modem" frame
            ModemMessage stitch(std::deque<ModemMessage>& in_frames);
            bool stitch_recursive(std::string& data, std::deque<ModemMessage>& in);
            bool unstitch_recursive(std::string& data, ModemMessage& message);
        
       
            // clears the destination and ack values for the packet to reset for next $CADRQ
            void clear_packet();

            // slave function to receive_incoming_modem_data that actually writes a piece of the message (called for each user-frame)
            bool publish_incoming_piece(ModemMessage message, const unsigned incoming_var_id);        
        
          private:
            QueueMsgFunc1 callback_ack;
            QueueMsgFunc1 callback_receive;
            QueueMsgFunc1 callback_receive_ccl;        
            QueueMsgFunc2 callback_ondemand;

            QueueMsgFunc1 callback_expire;
        
            QueueSizeFunc callback_qsize;
        
            unsigned modem_id_;
        
            std::map<QueueKey, Queue> queues_;

            std::string xml_schema_;

            std::ostream* os_;

            // map frame number onto %queue pointer that contains
            // the data for this ack
            std::multimap<unsigned, Queue*> waiting_for_ack_;

            // the first *user* frame sets the tone (dest & ack) for the entire packet (all %modem frames)
            unsigned packet_ack_;
        };

        /// outputs information about all available messages (same as std::string summary())
        std::ostream& operator<< (std::ostream& out, const QueueManager& d);

    }

}

#endif
