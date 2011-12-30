// copyright 2010-2011 t. schneider tes@mit.edu
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
// along with this software.  If not, see <http://www.gnu.org/licenses/>

#ifndef CoreHelpers20110411H
#define CoreHelpers20110411H

#include "goby/pb/protobuf/app_base_config.pb.h"
#include "goby/util/protobuf/logger.pb.h"
#include <boost/program_options.hpp>


namespace goby
{
    namespace core
    {
        /// provides stream output operator for Google Protocol Buffers Message 
        inline std::ostream& operator<<(std::ostream& out, const google::protobuf::Message& msg)
        {
            return (out << "### " << msg.GetDescriptor()->full_name() << " ###\n" << msg.DebugString());
        }


        inline void merge_app_base_cfg(AppBaseConfig* base_cfg,
                                       const boost::program_options::variables_map& var_map)
        {
            if(var_map.count("ncurses"))
            {
                base_cfg->mutable_glog_config()->set_tty_verbosity(util::protobuf::GLogConfig::GUI);
            }
            else if (var_map.count("verbose"))
            {
                switch(var_map["verbose"].as<std::string>().size())
                {
                    default:
                    case 0:
                        base_cfg->mutable_glog_config()->set_tty_verbosity(util::protobuf::GLogConfig::VERBOSE);
                        break;
                    case 1:
                        base_cfg->mutable_glog_config()->set_tty_verbosity(util::protobuf::GLogConfig::DEBUG1);
                        break;
                    case 2:
                        base_cfg->mutable_glog_config()->set_tty_verbosity(util::protobuf::GLogConfig::DEBUG2);
                        break;
                    case 3:
                        base_cfg->mutable_glog_config()->set_tty_verbosity(util::protobuf::GLogConfig::DEBUG3);
                        break;
                }
            }

            if(var_map.count("no_db"))
                base_cfg->mutable_database_config()->set_using_database(false);
        }

    }
}



#endif