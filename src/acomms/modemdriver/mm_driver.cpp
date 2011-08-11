// copyright 2009-2011 t. schneider tes@mit.edu
// copyright 2011 j. walls
// 
// this file is part of the goby-acomms WHOI Micro-Modem driver.
// goby-acomms is a collection of libraries 
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

#include <sstream>

#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <boost/algorithm/string.hpp>

#include "goby/util/logger.h"
#include "goby/util/binary.h"

#include "mm_driver.h"
#include "driver_exception.h"

using goby::util::NMEASentence;
using goby::util::goby_time;
using goby::util::hex_encode;
using goby::util::hex_decode;
using goby::util::ptime2unix_double;
using goby::util::as;
using google::protobuf::uint32;
using namespace goby::util::tcolor;


boost::posix_time::time_duration goby::acomms::MMDriver::MODEM_WAIT = boost::posix_time::seconds(3);
boost::posix_time::time_duration goby::acomms::MMDriver::WAIT_AFTER_REBOOT = boost::posix_time::seconds(2);
int goby::acomms::MMDriver::ALLOWED_MS_DIFF = 2000;
boost::posix_time::time_duration goby::acomms::MMDriver::HYDROID_GATEWAY_GPS_REQUEST_INTERVAL = boost::posix_time::seconds(30);
std::string goby::acomms::MMDriver::SERIAL_DELIMITER = "\r";
unsigned goby::acomms::MMDriver::PACKET_FRAME_COUNT [] = { 1, 3, 3, 2, 2, 8 };
unsigned goby::acomms::MMDriver::PACKET_SIZE [] = { 32, 32, 64, 256, 256, 256 };


//
// INITIALIZATION
//

goby::acomms::MMDriver::MMDriver(std::ostream* log /*= 0*/)
    : ModemDriverBase(log),
      log_(log),
      last_write_time_(goby_time()),
      waiting_for_modem_(false),
      startup_done_(false),
      global_fail_count_(0),
      present_fail_count_(0),
      clock_set_(false),
      last_hydroid_gateway_gps_request_(goby_time()),
      is_hydroid_gateway_(false),
      local_cccyc_(false)
{
    initialize_talkers();
}

void goby::acomms::MMDriver::startup(const protobuf::DriverConfig& cfg)
{
    if(startup_done_)
    {
        if(log_) *log_ << warn << group("modem_out") << "startup() called but driver is already started." << std::endl;
        return;
    }
        
    // store a copy for us later
    driver_cfg_ = cfg;
    
    if(!cfg.has_line_delimiter())
        driver_cfg_.set_line_delimiter(SERIAL_DELIMITER);

    if(!cfg.has_serial_baud())
        driver_cfg_.set_serial_baud(DEFAULT_BAUD);

    // support the non-standard Hydroid gateway buoy
    if(driver_cfg_.HasExtension(MicroModemConfig::hydroid_gateway_id))
        set_hydroid_gateway_prefix(driver_cfg_.GetExtension(MicroModemConfig::hydroid_gateway_id));

    
    modem_start(driver_cfg_);

    // some clock stuff -- set clk_mode_ zeros for starters
    set_clock();
    clk_mode_ = 0;

    write_cfg();
    
    // so that we know what the modem has for all the NVRAM values, not just the ones we set
    query_all_cfg();

    startup_done_ = true;
}




void goby::acomms::MMDriver::initialize_talkers()
{
    boost::assign::insert (sentence_id_map_)
        ("ACK",ACK)("DRQ",DRQ)("RXA",RXA)("RXD",RXD)
        ("RXP",RXP)("TXD",TXD)("TXA",TXA)("TXP",TXP) 
        ("TXF",TXF)("CYC",CYC)("MPC",MPC)("MPA",MPA)
        ("MPR",MPR)("RSP",RSP)("MSC",MSC)("MSA",MSA)
        ("MSR",MSR)("EXL",EXL)("MEC",MEC)("MEA",MEA) 
        ("MER",MER)("MUC",MUC)("MUA",MUA)("MUR",MUR) 
        ("PDT",PDT)("PNT",PNT)("TTA",TTA)("MFD",MFD) 
        ("CLK",CLK)("CFG",CFG)("AGC",AGC)("BBD",BBD) 
        ("CFR",CFR)("CST",CST)("MSG",MSG)("REV",REV) 
        ("DQF",DQF)("SHF",SHF)("MFD",MFD)("SNR",SNR) 
        ("DOP",DOP)("DBG",DBG)("FFL",FFL)("FST",FST) 
        ("ERR",ERR)("TOA",TOA)("XST",XST);

    boost::assign::insert (talker_id_map_)
        ("CC",CC)("CA",CA)("SN",SN)("GP",GP); 

    // from Micro-Modem Software Interface Guide v. 3.04
    boost::assign::insert (description_map_)
        ("$CAACK","Acknowledgment of a transmitted packet")
        ("$CADRQ","Data request message, modem to host")
        ("$CARXA","Received ASCII message, modem to host")
        ("$CARXD","Received binary message, modem to host")
        ("$CARXP","Incoming packet detected, modem to host")
        ("$CCTXD","Transmit binary data message, host to modem")
        ("$CCTXA","Transmit ASCII data message, host to modem")
        ("$CATXD","Echo back of transmit binary data message")
        ("$CATXA","Echo back of transmit ASCII data message")
        ("$CATXP","Start of packet transmission, modem to host")
        ("$CATXF","End of packet transmission, modem to host")
        ("$CCCYC","Network Cycle Initialization Command")
        ("$CACYC","Echo of Network Cycle Initialization command")
        ("$CCMPC","Mini-Packet Ping command, host to modem")
        ("$CAMPC","Echo of Ping command, modem to host")
        ("$CAMPA","A Ping has been received, modem to host")
        ("$CAMPR","Reply to Ping has been received, modem to host")
        ("$CCRSP","Pinging with an FM sweep")
        ("$CARSP","Respose to FM sweep ping command")
        ("$CCMSC","Sleep command, host to modem")
        ("$CAMSC","Echo of Sleep command, modem to host")
        ("$CAMSA","A Sleep was received acoustically, modem to host")
        ("$CAMSR","A Sleep reply was received, modem to host")
        ("$CCEXL","External hardware control command, local modem only")
        ("$CCMEC","External hardware control command, host to modem")
        ("$CAMEC","Echo of hardware control command, modem to host")
        ("$CAMEA","Hardware control command received acoustically")
        ("$CAMER","Hardware control command reply received")
        ("$CCMUC","User Mini-Packet command, host to modem")
        ("$CAMUC","Echo of user Mini-Packet, modem to host")
        ("$CAMUA","Mini-Packet received acoustically, modem to host")
        ("$CAMUR","Reply to Mini-Packet received, modem to host")
        ("$CCPDT","Ping REMUS digital transponder, host to modem")
        ("$CCPNT","Ping narrowband transponder, host to modem")
        ("$SNTTA","Transponder travel times, modem to host")
        ("$SNMFD","Nav matched filter information, modem to host")
        ("$CCCLK","Set clock, host to modem")
        ("$CCCFG","Set NVRAM configuration parameter, host to modem")
        ("$CCCFQ","Query configuration parameter, host to modem")
        ("$CCAGC","Set automatic gain control")
        ("$CABBD","Dump of baseband data to serial port, modem to host")
        ("$CCCFR","Measure noise level at receiver, host to modem")
        ("$SNCFR","Noise report, modem to host")
        ("$CACST","Communication cycle receive statistics")
        ("$CAXST","Communication cycle transmit statistics")
        ("$CAMSG","Transaction message, modem to host")
        ("$CAREV","Software revision message, modem to host")
        ("$CADQF","Data quality factor information, modem to host")
        ("$CASHF","Shift information, modem to host")
        ("$CAMFD","Comms matched filter information, modem to host")
        ("$CACLK","Time/Date message, modem to host")
        ("$CASNR","SNR statistics on the incoming PSK packet")
        ("$CADOP","Doppler speed message, modem to host")
        ("$CADBG","Low level debug message, modem to host")
        ("$CAERR","Error message, modem to host")
        ("$CATOA","Message from modem to host reporting time of arrival of the previous packet, and the synchronous timing mode used to determine that time.");

    // from Micro-Modem Software Interface Guide v. 3.04
    boost::assign::insert (cfg_map_)
        ("AGC","Turn on automatic gain control")
        ("AGN","Analog Gain (50 is 6 dB, 250 is 30 dB)")
        ("ASD","Always Send Data. Tells the modem to send test data when the user does not provide any.")
        ("BBD","PSK Baseband data dump to serial port")
        ("BND","Frequency Bank (1, 2, 3 for band A, B, or C, 0 for user-defined PSK only band)")
        ("BR1","Baud rate for serial port 1 (3 = 19200)")
        ("BR2","Baud rate for serial port 2 (3 = 19200)")
        ("BRN","Run bootloader at next revert")
        ("BSP","Boot loader serial port")
        ("BW0","Bandwidth for Band 0 PSK CPR 0-1 Coprocessor power toggle switch 1")
        ("CRL","Cycle init reverb lockout (ms) 50")
        ("CST","Cycle statistics message 1")
        ("CTO","Cycle init timeout (sec) 10")
        ("DBG","Enable low-level debug messages 0")
        ("DGM","Diagnostic messaging 0")
        ("DOP","Whether or not to send the $CADOP message")
        ("DQF","Whether or not to send the $CADQF message")
        ("DTH","Matched filter signal threshold, FSK")
        ("DTO","Data request timeout (sec)")
        ("DTP","Matched filter signal threshold, PSK")
        ("ECD","Int Delay at end of cycle (ms)")
        ("EFF","Feedforward taps for the LMS equalizer")
        ("EFB","Feedback taps for the LMS equalizer")
        ("FMD","PSK FM probe direction,0 up, 1 down")
        ("FML","PSK FM probe length, symbols")
        ("FC0","Carrier at Band 0 PSK only")
        ("GPS","GPS parser on aux. serial port")
        ("HFC","Hardware flow control on main serial port")
        ("MCM","Enable current mode hydrophone power supply on Rev. C Multi-Channel Analog Board. Must be set to 1 for Rev. B Multi-Channel Analog Board.")
        ("MFD","Whether or not to send the MFD messages")
        ("IRE","Print impulse response of FM sweep")
        ("MFC","MFD calibration value (samples)")
        ("MFD","Whether or not to send the MFD messages")
        ("MOD","0 sends FSK minipacket, 1 sends PSK minipacket")
        ("MPR","Enable power toggling on Multi-Channel Analog Board")
        ("MSE","Print symbol mean squared error (dB) from the LMS equalizer")
        ("MVM","Enable voltage mode hydrophone power supply on Multi-Channel Analog Board")
        ("NDT","Detect threshold for nav detector") 
        ("NPT","Power threshold for nav detector")
        ("NRL","Navigation reverb lockout (ms)")
        ("NRV","Number of CTOs before hard reboot")
        ("PAD","Power-amp delay (ms)")
        ("PCM","Passband channel mask")
        ("POW","Detection power threshold (dB) PRL Int Packet reverb lockout (ms)")
        ("PTH","Matched filter detector power threshold")
        ("PTO","Packet timeout (sec)")
        ("REV","Whether or not to send the $CAREV message")
        ("SGP","Show GPS messages on main serial port")
        ("RXA","Whether or not to send the $CARXA message")
        ("RXD","Whether or not to send the $CARXD message")
        ("RXP","Whether or not to send the $CARXP message") 
        ("SCG","Set clock from GPS")
        ("SHF","Whether or not to send the $CASHF message")
        ("SNR","Turn on SNR stats for PSK comms")
        ("SNV","Synchronous transmission of packets")
        ("SRC","Default Source Address")
        ("TAT","Navigation turn-around-time (msec)")
        ("TOA","Display time of arrival of a packet (sec)")
        ("TXD","Delay before transmit (ms)")
        ("TXP","Turn on start of transmit message")
        ("TXF","Turn on end of transmit message")
        ("XST","Turn on transmit stats message, CAXST");    
    
    
}

void goby::acomms::MMDriver::set_hydroid_gateway_prefix(int id)
{
    is_hydroid_gateway_ = true;
    // If the buoy is in use, make the prefix #M<ID>
    hydroid_gateway_gps_request_ = "#G" + as<std::string>(id) + "\r\n";        
    hydroid_gateway_modem_prefix_ = "#M" + as<std::string>(id);
    
    if(log_) *log_ << "Setting the hydroid_gateway buoy prefix: out=" << hydroid_gateway_modem_prefix_ << std::endl;
}


void goby::acomms::MMDriver::set_clock()
{
    NMEASentence nmea("$CCCLK", NMEASentence::IGNORE);
    boost::posix_time::ptime p = goby_time();

    // for sync nav, let's make sure to send the ccclk at the beginning of the
    // second: between 1ms-50ms after the top of the second
    // see WHOI sync nav manual
    // http://acomms.whoi.edu/documents/Synchronous%20Navigation%20With%20MicroModem%20RevD.pdf
    double frac_sec = double(p.time_of_day().fractional_seconds())/p.time_of_day().ticks_per_second();
    while(frac_sec > 50e-3 || frac_sec < 1e-3)
    {
        // wait 1 ms
        usleep(1000);
        p = goby_time();
        frac_sec = double(p.time_of_day().fractional_seconds())/p.time_of_day().ticks_per_second();
    }
    
    nmea.push_back(int(p.date().year()));
    nmea.push_back(int(p.date().month()));
    nmea.push_back(int(p.date().day()));
    nmea.push_back(int(p.time_of_day().hours()));
    nmea.push_back(int(p.time_of_day().minutes()));
    nmea.push_back(int(p.time_of_day().seconds()));
    
    static protobuf::ModemMsgBase base_msg;
    base_msg.Clear();
    base_msg.set_time(as<std::string>(p));
    append_to_write_queue(nmea, &base_msg);

    // take a breath to let the clock be set 
    sleep(1);
}

void goby::acomms::MMDriver::write_cfg()
{
    // reset nvram if requested and not a Hydroid buoy
    // as this resets the baud to 19200 and the buoy
    // requires 4800
    if(!is_hydroid_gateway_ && driver_cfg_.GetExtension(MicroModemConfig::reset_nvram))
        write_single_cfg("ALL,0");

    write_single_cfg("SRC," + as<std::string>(driver_cfg_.modem_id()));
    
    
    for(int i = 0, n = driver_cfg_.ExtensionSize(MicroModemConfig::nvram_cfg); i < n; ++i)
    {
        write_single_cfg(driver_cfg_.GetExtension(MicroModemConfig::nvram_cfg, i));
    }    
}

void goby::acomms::MMDriver::write_single_cfg(const std::string &s)
{
        
    NMEASentence nmea("$CCCFG", NMEASentence::IGNORE);        
    nmea.push_back(boost::to_upper_copy(s));

    // set our map now so we know various values immediately (like SRC)
    nvram_cfg_[nmea[1]] = nmea.as<int>(2);
        
    static protobuf::ModemMsgBase base_msg;
    base_msg.Clear();
    append_to_write_queue(nmea, &base_msg);
    
}



void goby::acomms::MMDriver::query_all_cfg()
{
    NMEASentence nmea("$CCCFQ,ALL", NMEASentence::IGNORE);

    static protobuf::ModemMsgBase base_msg;
    base_msg.Clear();
    append_to_write_queue(nmea, &base_msg);
}


//
// SHUTDOWN
//


void goby::acomms::MMDriver::shutdown()
{
    startup_done_ = false;
    modem_close();
}

goby::acomms::MMDriver::~MMDriver()
{ }


//
// LOOP
//

void goby::acomms::MMDriver::do_work()
{    
    // don't try to set the clock if we already have outgoing
    // messages queued since the time will be wrong by the time
    // we can send
    if(!clock_set_ && out_.empty())
        set_clock();
    
    // keep trying to send stuff to the modem
    try_send();

    // read any incoming messages from the modem
    std::string in;
    while(modem_read(&in))
    {
        boost::trim(in);
	// Check for whether the hydroid_gateway buoy is being used and if so, remove the prefix
	if (is_hydroid_gateway_) in.erase(0, HYDROID_GATEWAY_PREFIX_LENGTH);

        // try to handle the received message, posting appropriate signals
        try
        {
            NMEASentence nmea(in, NMEASentence::VALIDATE);
            process_receive(nmea);
        }
        catch(std::exception& e)
        {
            if(log_) *log_ << group("modem_in") << warn << e.what() << std::endl;
        }
    }

    // if we're using a hydroid buoy query it for its GPS position
    if(is_hydroid_gateway_ &&
       last_hydroid_gateway_gps_request_ + HYDROID_GATEWAY_GPS_REQUEST_INTERVAL < goby_time())
    {
        modem_write(hydroid_gateway_gps_request_);
        last_hydroid_gateway_gps_request_ = goby_time();
    }
    
}

//
// HANDLE MAC SIGNALS
//

void goby::acomms::MMDriver::handle_initiate_transmission(protobuf::ModemDataInit* init_msg)
{
    switch(init_msg->GetExtension(MicroModem::init_slot).type())
    {
        case protobuf::SLOT_DATA:
        {
            // we initiated this cycle so don't grab data *again* on the CACYC (in cyc()) 
            local_cccyc_ = true;
            init_msg->set_num_frames(PACKET_FRAME_COUNT[init_msg->base().rate()]);
            cache_outgoing_data(*init_msg);
    
            // don't start a local cycle if we have no data
            const bool is_local_cycle = init_msg->base().src() == driver_cfg_.modem_id();
            if(!(is_local_cycle && cached_data_msgs_.empty()))
            {
                //$CCCYC,CMD,ADR1,ADR2,Packet Type,ACK,Npkt*CS
                NMEASentence nmea("$CCCYC", NMEASentence::IGNORE);
                nmea.push_back(0); // CMD: deprecated field
                nmea.push_back(init_msg->base().src()); // ADR1
                
        
                nmea.push_back(is_local_cycle
                               ? cached_data_msgs_.begin()->second.base().dest()
                               : init_msg->base().dest()); // ADR2        
        
                nmea.push_back(init_msg->base().rate()); // Packet Type (transmission rate)
                nmea.push_back(is_local_cycle
                               ? static_cast<int>(cached_data_msgs_.begin()->second.ack_requested())
                               : 1); // ACK: deprecated field, but still dictates the value provided by CADRQ
                nmea.push_back(init_msg->num_frames()); // number of frames we want

                append_to_write_queue(nmea, init_msg->mutable_base());
            }
            else
            {
                if(log_)
                    *log_ << group("modem_out") << "Not initiating transmission because we have no data to send" << std::endl;
            }
       }
        break;
        
        case protobuf::SLOT_MINI:
        {
            const int MINI_NUM_FRAMES = 1;
            init_msg->set_num_frames(MINI_NUM_FRAMES);
            cache_outgoing_data(*init_msg);

            std::map<unsigned, protobuf::ModemDataTransmission>::iterator it =
                cached_data_msgs_.find(0);
    
            if(it != cached_data_msgs_.end())
            {
                protobuf::ModemDataTransmission& data_msg = it->second;
                data_msg.mutable_data()->resize(MINI_PACKET_SIZE);
            
                if((data_msg.data()[0] & 0x1F) != data_msg.data()[0])
                {
                    if(log_)
                        *log_ << group("modem_out") << warn << "MINI transmission can only be 13 bits; top three bits passed were *not* zeros, so discarding. You should AND your two bytes with 0x1FFF to get 13 bits" << std::endl;
                    data_msg.mutable_data()->at(0) &= 0x1F;
                }

                //$CCMUC,SRC,DEST,HHHH*CS
                NMEASentence nmea("$CCMUC", NMEASentence::IGNORE);
                nmea.push_back(init_msg->base().src()); // ADR1
                nmea.push_back(init_msg->base().dest()); // ADR2
                nmea.push_back(goby::util::hex_encode(data_msg.data())); //HHHH    
                cached_data_msgs_.erase(it);

                
                append_to_write_queue(nmea, init_msg->mutable_base());
            }
            else
            {
                if(log_)
                    *log_ << group("modem_out") << warn << "MINI transmission failed: no data provided" << std::endl;
                break;
            }

        }
        break;

        default:
            if(log_)
                *log_ << group("modem_out") << warn << "Not initiating transmission because we were given an invalid SLOT type:" << init_msg->GetExtension(MicroModem::init_slot) << std::endl;
            break;
            
    }    
}

void goby::acomms::MMDriver::handle_initiate_ranging(protobuf::ModemRangingRequest* request_msg)
{
    switch(request_msg->type())
    {
        case protobuf::MODEM_ONE_WAY_SYNCHRONOUS:
        {
            if(log_) *log_ << warn << group("modem_out") << "Cannot initiate ONE_WAY_SYNCHRONOUS ping manually. You must enable NVRAM cfg \"TOA,1\" and \"SNV,1\" and one-way synchronous messages will be reported on all relevant acoustic transactions" << std::endl;
            break;
        }
        
        case protobuf::MODEM_TWO_WAY_PING:
        {
            //$CCMPC,SRC,DEST*CS
            NMEASentence nmea("$CCMPC", NMEASentence::IGNORE);
            nmea.push_back(request_msg->base().src()); // ADR1
            nmea.push_back(request_msg->base().dest()); // ADR2

            last_ranging_type_ = request_msg->type();
            
            append_to_write_queue(nmea, request_msg->mutable_base());
            break;
        }
        

        case protobuf::REMUS_LBL_RANGING:
        {
            uint32 tat = driver_cfg_.GetExtension(MicroModemConfig::remus_turnaround_ms);
            if(!nvram_cfg_["TAT"] == tat)
                write_single_cfg("TAT" + as<std::string>(tat));

            // $CCPDT,GRP,CHANNEL,SF,STO,Timeout,AF,BF,CF,DF*CS
            NMEASentence nmea("$CCPDT", NMEASentence::IGNORE);
            nmea.push_back(1); // GRP 1 is the only group right now 
            nmea.push_back(request_msg->base().src() % 4 + 1); // can only use 1-4
            nmea.push_back(0); // synchronize may not work?
            nmea.push_back(0); // synchronize may not work?
            // REMUS LBL is 50 ms turn-around time, assume 1500 m/s speed of sound
            nmea.push_back(int((request_msg->lbl_max_range()*2.0 / ROUGH_SPEED_OF_SOUND)*1000 + tat));
            nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::remus_enable_beacons) >> 0 & 1);
            nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::remus_enable_beacons) >> 1 & 1);
            nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::remus_enable_beacons) >> 2 & 1);
            nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::remus_enable_beacons) >> 3 & 1);

            last_ranging_type_ = request_msg->type();
            
            append_to_write_queue(nmea, request_msg->mutable_base());
            break;
        }

        case protobuf::NARROWBAND_LBL_RANGING:
        {
            uint32 tat = driver_cfg_.GetExtension(MicroModemConfig::narrowband_turnaround_ms);
            if(!nvram_cfg_["TAT"] == tat)
                write_single_cfg("TAT" + as<std::string>(tat));

            // $CCPNT, Ftx, Ttx, Trx, Timeout, FA, FB, FC, FD,Tflag*CS
            NMEASentence nmea("$CCPNT", NMEASentence::IGNORE);
            nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::narrowband_transmit_freq));
            nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::narrowband_transmit_ping_ms));
            nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::narrowband_receive_ping_ms));
            nmea.push_back(int((request_msg->lbl_max_range()*2.0 / ROUGH_SPEED_OF_SOUND)*1000 + tat));

            // no more than four allowed
            const int MAX_NUMBER_RX_BEACONS = 4;
            
            int number_rx_freq_provided = std::min(MAX_NUMBER_RX_BEACONS, driver_cfg_.ExtensionSize(MicroModemConfig::narrowband_receive_freq));
            
            for(int i = 0, n = std::max(MAX_NUMBER_RX_BEACONS, number_rx_freq_provided); i < n; ++i)
            {
                if(i < number_rx_freq_provided)
                    nmea.push_back(driver_cfg_.GetExtension(MicroModemConfig::narrowband_receive_freq, i));
                else
                    nmea.push_back(0);
            }
            
            nmea.push_back((int)driver_cfg_.GetExtension(MicroModemConfig::narrowband_transmit_flag));

            last_ranging_type_ = request_msg->type();
            
            append_to_write_queue(nmea, request_msg->mutable_base());
            break;
        }
    }
}

//
// OUTGOING NMEA
//


void goby::acomms::MMDriver::try_send()
{
    if(out_.empty()) return;

    const protobuf::ModemMsgBase& base_msg = out_.front().second;
    
    bool resend = waiting_for_modem_ && (last_write_time_ <= (goby_time() - MODEM_WAIT));
    if(!waiting_for_modem_)
    {
        mm_write(base_msg);
    }
    else if(resend)
    {
        if(log_) *log_ << group("modem_out") << warn << "resending last command; no serial ack in " << (goby_time() - last_write_time_).total_seconds() << " second(s). " << std::endl;
        ++global_fail_count_;
        
        if(global_fail_count_ == MAX_FAILS_BEFORE_DEAD)
        {
            modem_close();
            throw(ModemDriverException("modem appears to not be responding!"));
        }
        
        try
        {
            // try to increment the present (current NMEA sentence) fail counter
            // will throw if fail counter exceeds RETRIES
            increment_present_fail();
            // assuming we're still ok, write the line again
            mm_write(base_msg);
        }
        catch(ModemDriverException& e)
        {
            present_fail_exceeds_retries();
        }
    }
}

void goby::acomms::MMDriver::increment_present_fail()
{
    ++present_fail_count_;
    if(present_fail_count_ >= RETRIES)
        throw(ModemDriverException("Fail count exceeds RETRIES"));
}

void goby::acomms::MMDriver::present_fail_exceeds_retries()
{
    if(log_) *log_  << group("modem_out") << warn << "modem did not respond to our command even after " << RETRIES << " retries. continuing onwards anyway..." << std::endl;
    pop_out();    
}



void goby::acomms::MMDriver::mm_write(const protobuf::ModemMsgBase& base_msg)
{
    if(log_) *log_ << group("modem_out") << hydroid_gateway_modem_prefix_
                   << base_msg.raw() << "\n" << "^ "
                   << magenta << base_msg.description() << nocolor << std::endl;

    signal_all_outgoing(base_msg);    
 
    modem_write(hydroid_gateway_modem_prefix_ + base_msg.raw() + "\r\n");

    waiting_for_modem_ = true;
    last_write_time_ = goby_time();
}




void goby::acomms::MMDriver::pop_out()
{
    waiting_for_modem_ = false;

    if(!out_.empty())
        out_.pop_front();
    else
    {
        if(log_) *log_ << group("modem_out") << warn
                       << "Expected to pop outgoing NMEA message but out_ deque is empty" << std::endl;
    }
    
    present_fail_count_ = 0;
}

void goby::acomms::MMDriver::append_to_write_queue(const NMEASentence& nmea, protobuf::ModemMsgBase* base_msg)
{
    base_msg->set_raw(nmea.message());
    
    if(!base_msg->has_time())
        base_msg->set_time(as<std::string>(goby_time()));
    
    if(!base_msg->has_description() && description_map_.count(nmea.front()))
        base_msg->set_description(description_map_[nmea.front()]);

    
    out_.push_back(make_pair(nmea, *base_msg));
    try_send(); // try to push it now without waiting for the next call to do_work();
}

//
// INCOMING NMEA
//


void goby::acomms::MMDriver::process_receive(const NMEASentence& nmea)
{
    // need to print this first so raw log messages appear causal (as they are)
    if(log_)
    {
        *log_ << group("modem_in") << nmea.message() << std::endl;
        if(description_map_.count(nmea.front()))
            *log_ << group("modem_in") << "^ " << blue << description_map_[nmea.front()] << nocolor << std::endl;
    }

    global_fail_count_ = 0;
    
    protobuf::ModemMsgBase* this_base_msg = 0;
    static protobuf::ModemMsgBase base_msg;
    static protobuf::ModemDataInit init_msg;
    static protobuf::ModemDataTransmission data_msg;
    static protobuf::ModemDataAck ack_msg;
    static protobuf::ModemRangingReply ranging_msg;
    
    base_msg.Clear();
    // look at the sentence id (last three characters of the NMEA 0183 talker)
    switch(sentence_id_map_[nmea.sentence_id()])
    {
        //
        // local modem
        //
        case REV: rev(nmea); break; // software revision
        case ERR: err(nmea); break; // error message
        case DRQ: drq(nmea); break; // data request
        case CFG: cfg(nmea, &base_msg); break; // configuration
        case CLK: clk(nmea, &base_msg); break; // clock
        case XST: xst(nmea); break; // transmit stats for clock mode

            
        //
        // data cycle
        //
        case CYC:  // cycle init
        {
            init_msg.Clear();
            cyc(nmea, &init_msg);
            this_base_msg = init_msg.mutable_base();
            // can't trust ADR1 to be SRC, so we trash this CATOA
            ranging_msg.Clear();
            break;
        }

        case RXD:  // data receive
        {
            data_msg.Clear();
            rxd(nmea, &data_msg);
            this_base_msg = data_msg.mutable_base();
            flush_toa(*this_base_msg, &ranging_msg);
            break;
        }

        
        case MUA:  // mini-packet receive
        {
            data_msg.Clear();
            mua(nmea, &data_msg);
            this_base_msg = data_msg.mutable_base(); 
            flush_toa(*this_base_msg, &ranging_msg);
            break;
        }
 

        
        case ACK:  // acknowledge
        {
            ack_msg.Clear();
            ack(nmea, &ack_msg);
            this_base_msg = ack_msg.mutable_base();
            flush_toa(*this_base_msg, &ranging_msg);
            break;
        }

        //
        // ranging
        //
        case MPR:  // ping response
        {
            ranging_msg.Clear();
            mpr(nmea, &ranging_msg);
            this_base_msg = ranging_msg.mutable_base();
            break;
        }
        
        case TTA: // remus lbl times
        {
            ranging_msg.Clear();
            tta(nmea, &ranging_msg);
            this_base_msg = ranging_msg.mutable_base();
            break; 
        }
        
        case TOA: // one way synchronous Time-Of-Arrival
        {
            ranging_msg.Clear();
            toa(nmea, &ranging_msg);
            this_base_msg = ranging_msg.mutable_base();
            break; 
        }
        case RXP:
        {
            // clear out any pending TOA that didn't get flushed
            if(ranging_msg.type() == protobuf::MODEM_ONE_WAY_SYNCHRONOUS)
            {
                if(log_) *log_ << group("modem_in") << warn << "failed to flush: " << ranging_msg << std::endl;
                ranging_msg.Clear();
            }
        }    

        
        default: break;
    }

    if(!this_base_msg) this_base_msg = &base_msg;

    if(log_ && this_base_msg->has_description())
        *log_ << group("modem_in") << "^ " << blue << this_base_msg->description() << nocolor << std::endl;
    
    this_base_msg->set_raw(nmea.message());
    if(!this_base_msg->has_description() && description_map_.count(nmea.front()))
        this_base_msg->set_description(description_map_[nmea.front()]);
    
    signal_all_incoming(*this_base_msg);
    
    // clear the last send given modem acknowledgement
    if(!out_.empty() && out_.front().first.sentence_id() == nmea.sentence_id())
        pop_out();    
}

void goby::acomms::MMDriver::ack(const NMEASentence& nmea, protobuf::ModemDataAck* m)
{
    uint32 frame = as<uint32>(nmea[3])-1;
    
    if(frames_waiting_for_ack_.count(frame))
    {
        
        m->mutable_base()->set_time(as<std::string>(goby_time()));
        m->mutable_base()->set_src(as<uint32>(nmea[1]));
        m->mutable_base()->set_dest(as<uint32>(nmea[2]));
        // WHOI counts starting at 1, Goby counts starting at 0
        m->set_frame(frame);
        
        signal_ack(*m);

        frames_waiting_for_ack_.erase(frame);
    }
    else
    {
        if(log_)
            *log_ << warn << "Received acknowledgement for Micro-Modem frame " << frame + 1 << " (Goby frame " << frame << ") that we were not expecting." << std::endl;
    }
}

void goby::acomms::MMDriver::drq(const NMEASentence& nmea_in)
{
    //$CADRQ,HHMMSS,SRC,DEST,ACK,N,F#*CS

    NMEASentence nmea_out("$CCTXD", NMEASentence::IGNORE);        

    // WHOI counts frames from 1, we count from 0
    int frame = as<int>(nmea_in[6])-1;
    std::map<unsigned, protobuf::ModemDataTransmission>::iterator it =
        cached_data_msgs_.find(frame);
    
    if(it != cached_data_msgs_.end())
    {
        // use the cached data
        protobuf::ModemDataTransmission& data_msg = it->second;
        nmea_out.push_back(data_msg.base().src());
        nmea_out.push_back(data_msg.base().dest());
        nmea_out.push_back(int(data_msg.ack_requested()));
        nmea_out.push_back(hex_encode(data_msg.data()));
        cached_data_msgs_.erase(it);
        
        if(data_msg.ack_requested())
            frames_waiting_for_ack_.insert(frame);
    }
    else
    {
        // send a blank message to supress further DRQ
        nmea_out.push_back(nmea_in[2]); // SRC
        nmea_out.push_back(nmea_in[3]); // DEST
        nmea_out.push_back(nmea_in[4]); // ACK
        nmea_out.push_back(""); // no data
    }
    static protobuf::ModemMsgBase base_msg;
    base_msg.Clear();
    append_to_write_queue(nmea_out, &base_msg);    
}

void goby::acomms::MMDriver::rxd(const NMEASentence& nmea, protobuf::ModemDataTransmission* m)
{
    m->mutable_base()->set_time(as<std::string>(goby_time()));
    m->mutable_base()->set_src(as<uint32>(nmea[1]));
    m->mutable_base()->set_dest(as<uint32>(nmea[2]));
    m->set_ack_requested(as<bool>(nmea[3]));
    // WHOI counts from 1, we count from 0
    m->set_frame(as<uint32>(nmea[4])-1);
    m->set_data(hex_decode(nmea[5]));
    
    m->SetExtension(MicroModem::packet_type, MicroModem::PACKET_DATA);
    
    signal_receive(*m);
}


void goby::acomms::MMDriver::mua(const NMEASentence& nmea, protobuf::ModemDataTransmission* m)
{
    m->mutable_base()->set_time(as<std::string>(goby_time()));
    m->mutable_base()->set_src(as<uint32>(nmea[1]));
    m->mutable_base()->set_dest(as<uint32>(nmea[2]));
    m->set_data(goby::util::hex_decode(nmea[3]));

    m->SetExtension(MicroModem::packet_type, MicroModem::PACKET_MINI);
    
    signal_receive(*m);
}

void goby::acomms::MMDriver::cfg(const NMEASentence& nmea, protobuf::ModemMsgBase* base_msg)
{
    nvram_cfg_[nmea[1]] = nmea.as<int>(2);
    base_msg->set_description("Configuration: " + cfg_map_[nmea.at(1)]);
    
    if(out_.empty() ||
       (out_.front().first.sentence_id() != "CFG" && out_.front().first.sentence_id() != "CFQ"))
        return;
    
    if(out_.front().first.sentence_id() == "CFQ") pop_out();
}

void goby::acomms::MMDriver::clk(const NMEASentence& nmea, protobuf::ModemMsgBase* base_msg)
{
    if(out_.empty() || out_.front().first.sentence_id() != "CLK")
        return;
    
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    // modem responds to the previous second, which is why we subtract one second from the current time
    ptime expected = goby_time();
    ptime reported = ptime(date(nmea.as<int>(1),
                                nmea.as<int>(2),
                                nmea.as<int>(3)),
                           time_duration(nmea.as<int>(4),
                                         nmea.as<int>(5),
                                         nmea.as<int>(6)+1,
                                         0));
    if(log_) *log_ << "reported time: " << reported << std::endl;
    
    
    base_msg->set_time(as<std::string>(reported));
    base_msg->set_time_source(protobuf::ModemMsgBase::MODEM_TIME);
    
    // make sure the modem reports its time as set at the right time
    // we may end up oversetting the clock, but better safe than sorry...
     boost::posix_time::time_duration t_diff = (reported - expected);
 
     if( abs( int( t_diff.total_milliseconds())) < ALLOWED_MS_DIFF )
         clock_set_ = true;    
}

void goby::acomms::MMDriver::xst(const NMEASentence& nmea)
{
    clk_mode_ = nmea.as<unsigned>(3);
}


void goby::acomms::MMDriver::mpr(const NMEASentence& nmea, protobuf::ModemRangingReply* m)
{
    m->mutable_base()->set_time(as<std::string>(goby_time()));
    
    // $CAMPR,SRC,DEST,TRAVELTIME*CS
    // reverse src and dest so they match the original request
    m->mutable_base()->set_src(as<uint32>(nmea[2]));
    m->mutable_base()->set_dest(as<uint32>(nmea[1]));

    if(nmea.size() > 3)
        m->add_one_way_travel_time(as<double>(nmea[3]));

    m->set_type(protobuf::MODEM_TWO_WAY_PING);

    
    signal_range_reply(*m);
}

void goby::acomms::MMDriver::rev(const NMEASentence& nmea)
{
    if(nmea[2] == "INIT")
    {
        // reboot
        sleep(WAIT_AFTER_REBOOT.total_seconds());
        clock_set_ = false;
    }
    else if(nmea[2] == "AUV")
    {
        //check the clock
        using boost::posix_time::ptime;
        ptime expected = goby_time();
        ptime reported = nmea_time2ptime(nmea[1]);

        boost::posix_time::time_duration t_diff = (reported - expected);
        
        if( abs( int( t_diff.total_milliseconds())) > ALLOWED_MS_DIFF )
            clock_set_ = false;
    }
}

void goby::acomms::MMDriver::err(const NMEASentence& nmea)
{
    *log_ << group("modem_out") << warn << "modem reports error: " << nmea.message() << std::endl;

    
    // recover quicker if old firmware does not understand one of our commands
    if(nmea.at(2) == "NMEA")
    {
        waiting_for_modem_ = false;

        try
        {
            increment_present_fail(); 
        }
        catch(ModemDriverException& e)
        {
            present_fail_exceeds_retries();
        }
    }
}



void goby::acomms::MMDriver::cyc(const NMEASentence& nmea, protobuf::ModemDataInit* init_msg)
{
    init_msg->mutable_base()->set_time(as<std::string>(goby_time()));

    // somewhat "loose" interpretation of some of the fields
    init_msg->mutable_base()->set_src(as<uint32>(nmea[2])); // ADR1
    init_msg->mutable_base()->set_dest(as<uint32>(nmea[3])); // ADR2
    init_msg->mutable_base()->set_rate(as<uint32>(nmea[4])); // Rate
    init_msg->set_num_frames(as<uint32>(nmea[6])); // Npkts, number of packets
    
    
    // if we're supposed to send and we didn't initiate the cycle
    if(!local_cccyc_)
        cache_outgoing_data(*init_msg);
    else // clear flag for next cycle
        local_cccyc_ = false;
}

void goby::acomms::MMDriver::cache_outgoing_data(const protobuf::ModemDataInit& init_msg)
{
    if(init_msg.base().src() == driver_cfg_.modem_id())
    {
        if(!cached_data_msgs_.empty())
        {
            if(log_) *log_ << warn << "flushing " << cached_data_msgs_.size() << " messages that were never sent in response to a $CADRQ." << std::endl;
            cached_data_msgs_.clear();
        }

        if(!frames_waiting_for_ack_.empty())
        {
            if(log_) *log_ << warn << "flushing " << frames_waiting_for_ack_.size() << " expected acknowledgments that were never received." << std::endl;
            frames_waiting_for_ack_.clear();
        }
        
    
        static protobuf::ModemDataRequest request_msg;
        request_msg.Clear();
        // make a data request in anticipation that we will need to send
        request_msg.mutable_base()->set_time(as<std::string>(goby_time()));
    
        request_msg.mutable_base()->set_src(init_msg.base().src());
        request_msg.mutable_base()->set_dest(init_msg.base().dest());
    
        // nmea_in[4] == ack

        protobuf::SlotType slot_type = init_msg.GetExtension(MicroModem::init_slot).type();
        if(slot_type == protobuf::SLOT_DATA)
            request_msg.set_max_bytes(PACKET_SIZE[init_msg.base().rate()]);
        else if(slot_type == protobuf::SLOT_MINI)
            request_msg.set_max_bytes(MINI_PACKET_SIZE);

        request_msg.MutableExtension(MicroModem::request_slot)->CopyFrom(init_msg.GetExtension(MicroModem::init_slot));

        // first copy over any frames provided 
        for(unsigned i = 0, n = init_msg.frame_size(); i < n; ++i)
        {
            cached_data_msgs_[i] = init_msg.frame(i);
        }
        
        // now signal data request for the rest
        static protobuf::ModemDataTransmission data_msg;
        for(unsigned i = init_msg.frame_size(), n = init_msg.num_frames(); i < n; ++i)
        {
            request_msg.set_frame(i);
            data_msg.Clear();
            
            signal_data_request(request_msg, &data_msg);

            if(!validate_data(request_msg, &data_msg))
               continue;

            // no more data to send
            if(data_msg.data().empty())
                break;
            
            cached_data_msgs_[i] = data_msg;
        }
    }
}



bool goby::acomms::MMDriver::validate_data(const protobuf::ModemDataRequest& request_msg,
                                           protobuf::ModemDataTransmission* data_msg)
{
    if(!data_msg->base().has_src())
    {
        data_msg->mutable_base()->set_src(request_msg.base().src());
    }
    else if(data_msg->base().src() != request_msg.base().src())
    {
        if(log_)
            *log_ << warn << "ModemDataTransmission::ModemMsgBase::src must equal ModemDataRequest::ModemMsgBase::dest" << std::endl;
        return false;
    }
            
    if(!data_msg->base().has_dest())
    {
        data_msg->mutable_base()->set_dest(request_msg.base().dest());
    }
    else if(!is_valid_destination(data_msg->base().dest()))
    {
        if(log_)
            *log_ << warn << "ModemDataTransmission::ModemMsgBase::src must equal ModemDataRequest::ModemMsgBase::dest" << std::endl;
        return false;
    }
    
    return true;
}


void goby::acomms::MMDriver::toa(const NMEASentence& nmea, protobuf::ModemRangingReply* m)
{
    // timing relative to synched pps is good, if ccclk is bad, and range is
    // known to be less than ~1500m, range is still usable
    clk_mode_ = nmea.as<unsigned>(2);
    if(clk_mode_ == protobuf::SYNC_TO_PPS_AND_CCCLK_GOOD ||
       clk_mode_ == protobuf::SYNC_TO_PPS_AND_CCCLK_BAD)
    {
        boost::posix_time::ptime toa = nmea_time2ptime(nmea[1]);
        double frac_sec = double(toa.time_of_day().fractional_seconds())/toa.time_of_day().ticks_per_second();

        m->add_one_way_travel_time(frac_sec);

        m->set_ambiguity(protobuf::ModemRangingReply::OWTT_SECOND_AMBIGUOUS);
        
        m->set_receiver_clk_mode((protobuf::ClockMode) clk_mode_);
        m->set_type(protobuf::MODEM_ONE_WAY_SYNCHRONOUS);
        m->mutable_base()->set_time(as<std::string>(toa));
        m->mutable_base()->set_time_source(protobuf::ModemMsgBase::MODEM_TIME);
    }
}

void goby::acomms::MMDriver::flush_toa(const protobuf::ModemMsgBase& base_msg, protobuf::ModemRangingReply* ranging_msg)
{
    if(ranging_msg->type() == protobuf::MODEM_ONE_WAY_SYNCHRONOUS)
    {
        ranging_msg->mutable_base()->set_dest(driver_cfg_.modem_id());
        ranging_msg->mutable_base()->set_src(base_msg.src());
        signal_range_reply(*ranging_msg);
        ranging_msg->Clear();
    }
}

void goby::acomms::MMDriver::tta(const NMEASentence& nmea, protobuf::ModemRangingReply* m)
{
    m->add_one_way_travel_time(as<double>(nmea[1]));
    m->add_one_way_travel_time(as<double>(nmea[2]));
    m->add_one_way_travel_time(as<double>(nmea[3]));
    m->add_one_way_travel_time(as<double>(nmea[4]));
    
    m->set_type(last_ranging_type_);

    m->mutable_base()->set_src(driver_cfg_.modem_id());
    m->mutable_base()->set_time(as<std::string>(nmea_time2ptime(nmea[5])));
    m->mutable_base()->set_time_source(protobuf::ModemMsgBase::MODEM_TIME);
    
    signal_range_reply(*m);    
}

//
// UTILITY
//

boost::posix_time::ptime goby::acomms::MMDriver::nmea_time2ptime(const std::string& mt)
{   
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    // must be at least HHMMSS
    if(mt.length() < 6)
        return ptime(not_a_date_time);  
    else
    {
        std::string s_hour = mt.substr(0,2), s_min = mt.substr(2,2), s_sec = mt.substr(4,2), s_fs = "0";

        // has some fractional seconds
        if(mt.length() > 7)
            s_fs = mt.substr(7); // everything after the "."
	        
        try
        {
            int hour = boost::lexical_cast<int>(s_hour);
            int min = boost::lexical_cast<int>(s_min);
            int sec = boost::lexical_cast<int>(s_sec);
            int micro_sec = boost::lexical_cast<int>(s_fs)*pow(10, 6-s_fs.size());
           
	    boost::gregorian::date return_date(boost::gregorian::day_clock::universal_day());
	    boost::posix_time::time_duration return_duration(boost::posix_time::time_duration(hour, min, sec, 0) + microseconds(micro_sec));
	    boost::posix_time::ptime return_time(return_date, return_duration);
            return return_time;
        }
        catch (boost::bad_lexical_cast&)
        {
            return ptime(not_a_date_time);
        }        
    }
}