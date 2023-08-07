/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Authors: Joe Kopena <tjkopena@cs.drexel.edu>
 * Modified by: Longhao Zou, Oct 2016 for EE500 <longhao.zou@dcu.ie>
 * Modified by: Andrey Golovanov, Jul 2023 for EE500 at DCU <networmix@gmail.com>
 *

            EE500 Assignment 2023
            Default WiFi Network Topology

                WiFi 192.168.0.0
            -------------------------
            |AP (node 0:192.168.0.1)|
            -------------------------
             *         *           *
            /          |            \
  Traffic 1/  Traffic 2|    ------   \ Traffic N
          /            |              \
      user 1       user 2     ------   user N
   (node 1         (node 2     ------  (node N
   :192.168.0.2    :192.168.0.3 ------ :192.168.0.N+1
   :1000)          :1001)        ------:1000+(N-1))
 *
 */

#include <ctime>
#include <sstream>
#include <iomanip> // Necessary for std::setw and std::setfill

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-phy.h"

#include "ee500_wifi_app.h"
#include "ee500_wifi_data.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ee500_WiFi_Sim");

// define struct WifiStatData to track WiFi data sent/received
struct WifiStatData
{
  Ptr<CounterCalculator<uint32_t>> mpduDropCount =
      CreateObject<CounterCalculator<uint32_t>>();
  Ptr<CounterCalculator<uint32_t>> mpduDropBytes =
      CreateObject<CounterCalculator<uint32_t>>();
  Ptr<CounterCalculator<uint32_t>> mpduRxCount =
      CreateObject<CounterCalculator<uint32_t>>();
  Ptr<CounterCalculator<uint32_t>> mpduRxBytes =
      CreateObject<CounterCalculator<uint32_t>>();
  Ptr<CounterCalculator<uint32_t>> mpduTxCount =
      CreateObject<CounterCalculator<uint32_t>>();
  Ptr<CounterCalculator<uint32_t>> mpduTxBytes =
      CreateObject<CounterCalculator<uint32_t>>();
  Ptr<CounterCalculator<double>> mpduRxRSSsum =
      CreateObject<CounterCalculator<double>>(); // sum of RSSI values for all received Data MPDUs (dBm)
};

void RxDropCallback(Mac48Address mac,
                    WifiStatData *wifiStatData,
                    Ptr<const Packet> packet, WifiPhyRxfailureReason reason)
{
  // packet here is a single MPDU
  Ptr<Packet> copy = packet->Copy();
  WifiMacHeader macHeader;
  copy->RemoveHeader(macHeader);

  // Assuming AP is the first created device (Node 0, Device 0)
  Mac48Address apMac = Mac48Address::ConvertFrom(NodeList::GetNode(0)->GetDevice(0)->GetAddress());

  std::string reasonStr;

  switch (reason)
  {
  case UNKNOWN:
    reasonStr = "UNKNOWN";
    break;
  case UNSUPPORTED_SETTINGS:
    reasonStr = "UNSUPPORTED_SETTINGS";
    break;
  case NOT_ALLOWED:
    reasonStr = "NOT_ALLOWED";
    break;
  case ERRONEOUS_FRAME:
    reasonStr = "ERRONEOUS_FRAME";
    break;
  case MPDU_WITHOUT_PHY_HEADER:
    reasonStr = "MPDU_WITHOUT_PHY_HEADER";
    break;
  case PREAMBLE_DETECT_FAILURE:
    reasonStr = "PREAMBLE_DETECT_FAILURE";
    break;
  case L_SIG_FAILURE:
    reasonStr = "L_SIG_FAILURE";
    break;
  case SIG_A_FAILURE:
    reasonStr = "SIG_A_FAILURE";
    break;
  case PREAMBLE_DETECTION_PACKET_SWITCH:
    reasonStr = "PREAMBLE_DETECTION_PACKET_SWITCH";
    break;
  case FRAME_CAPTURE_PACKET_SWITCH:
    reasonStr = "FRAME_CAPTURE_PACKET_SWITCH";
    break;
  case OBSS_PD_CCA_RESET:
    reasonStr = "OBSS_PD_CCA_RESET";
    break;
  default:
    reasonStr = "UNKNOWN_REASON";
    break;
  }

  // For packets received by STAs from the AP
  // Mac1 = destination, Mac2 = BSSID, Mac3 = original source
  // BSSID = AP MAC
  if (macHeader.IsData() && macHeader.GetAddr3() == apMac && mac == macHeader.GetAddr1())
  {
    NS_LOG_LOGIC("RxDrop at " << Simulator::Now().GetSeconds() << ", Reason: " << reasonStr);
    wifiStatData->mpduDropCount->Update();
    wifiStatData->mpduDropBytes->Update(packet->GetSize());
    wifiStatData->mpduTxCount->Update();
    wifiStatData->mpduTxBytes->Update(packet->GetSize());
  }
}

void MonitorSniffRxCallback(Mac48Address mac,
                            WifiStatData *wifiStatData,
                            Ptr<const Packet> packet, uint16_t channelFreqMhz,
                            WifiTxVector txVector, MpduInfo aMpdu,
                            SignalNoiseDbm signalNoise)
{
  // packet here can be a single MPDU or an A-MPDU
  Ptr<Packet> pktCopy = packet->Copy();
  if (IsAmpdu(packet))
  {
    // we received A-MPDU
    std::list<Ptr<const Packet>> mpdus = MpduAggregator::PeekMpdus(pktCopy);
    size_t numberOfMpdus = mpdus.size();
    NS_LOG_LOGIC("A-MPDU received, number of MPDUs: " << numberOfMpdus);
    for (auto &mpdu : mpdus)
    {
      Ptr<Packet> mpduCopy = mpdu->Copy();
      WifiMacHeader macHeader;
      mpduCopy->RemoveHeader(macHeader);

      // Assuming AP is the first created device (Node 0, Device 0)
      Mac48Address apMac = Mac48Address::ConvertFrom(NodeList::GetNode(0)->GetDevice(0)->GetAddress());

      // For packets received by STAs from the AP
      // Mac1 = destination, Mac2 = BSSID, Mac3 = original source
      // BSSID = AP MAC
      if (macHeader.IsData() && macHeader.GetAddr3() == apMac && mac == macHeader.GetAddr1())
      {
        NS_LOG_LOGIC("\tMPDU size: " << mpdu->GetSize());
        wifiStatData->mpduRxCount->Update();
        wifiStatData->mpduRxBytes->Update(mpdu->GetSize());
        wifiStatData->mpduTxCount->Update();
        wifiStatData->mpduTxBytes->Update(mpdu->GetSize());
        // Calculate the Recieved Signal Strength Indicator (RSSI) in dBm
        wifiStatData->mpduRxRSSsum->Update(signalNoise.signal);
      }
    }
  }
  else
  {
    NS_LOG_LOGIC("MPDU received");
    WifiMacHeader macHeader;
    pktCopy->RemoveHeader(macHeader);

    // Assuming AP is the first created device (Node 0, Device 0)
    Mac48Address apMac = Mac48Address::ConvertFrom(NodeList::GetNode(0)->GetDevice(0)->GetAddress());

    // For packets received by STAs from the AP
    // Mac1 = destination, Mac2 = BSSID, Mac3 = original source
    // BSSID = AP MAC
    if (macHeader.IsData() && macHeader.GetAddr3() == apMac && mac == macHeader.GetAddr1())
    {
      NS_LOG_LOGIC("\tMPDU size: " << pktCopy->GetSize());
      wifiStatData->mpduRxCount->Update();
      wifiStatData->mpduRxBytes->Update(pktCopy->GetSize());
      wifiStatData->mpduTxCount->Update();
      wifiStatData->mpduTxBytes->Update(pktCopy->GetSize());
      // Calculate the Recieved Signal Strength Indicator (RSSI) in dBm
      wifiStatData->mpduRxRSSsum->Update(signalNoise.signal);
    }
  }
}

int main(int argc, char *argv[])
{
  LogComponentEnable("ee500_WiFi_Sim", LOG_LEVEL_WARN);
  double distance = 10.0;
  double duration = 30;                             // Simulation Running Time (in seconds)
  uint64_t desiredDataRate = 800;                   // desired application data rate in kbps
  uint64_t packetSize = 1000;                       // packet size in bytes
  uint64_t packetNum = 1000000000;                  // number of packets to send
  bool verbose = false;                             // enable/disable verbose log messages to stdout
  bool pcap = false;                                // enable/disable pcap traces
  uint32_t staNum = 1;                              // number of WiFi Users
  bool debug = false;                               // enable/disable debug log messages to stdout
  std::string standard = "ac";                      // WiFi standard [b|g|a|n|ac|ax|ax24|n24]
  double lossExp = 3.0;                             // path loss exponent
  double TxPowerStart = -100;                       // start of Tx power range in dBm, if -100, use default
  double TxPowerEnd = -100;                         // end of Tx power range in dBm, if -100, use default
  double TxPowerLevels = 1.0;                       // number of Tx power levels
  std::string experiment("EE500_WiFi_Performance"); // the name of the experiment
  double channelWidth = 20;                         // channel width in MHz, default is 20 MHz
  std::string strategy("wifi-linear");              // By default, place STAs in a line along the x-axis
  std::string runID = "run-" + std::to_string(time(NULL));
  std::string input = "";                 // input for the experiment
  std::string distancesStr = "";          // comma separated list of distances
  std::string rateControl = "minstrelht"; // rate control algorithm
  std::string phyRate = "VhtMcs0";        // physical rate or "DataMode" for constant rate control

  // Set up command line parameters used to control the experiment
  CommandLine cmd;
  cmd.AddValue("distance", "Distance apart to place nodes (in meters).", distance);
  cmd.AddValue("duration", "Experiment duration (in seconds).", duration);
  cmd.AddValue("experiment", "Identifier for experiment.", experiment);
  cmd.AddValue("strategy", "Identifier for strategy [wifi-radial|wifi-linear].", strategy);
  cmd.AddValue("runID", "Identifier for run.", runID);
  cmd.AddValue("input", "Input for the experiment.", input);
  cmd.AddValue("verbose", "Enable/disable log messages to stdout.", verbose);
  cmd.AddValue("pcap", "Enable/disable pcap traces.", pcap);
  cmd.AddValue("staNum", "Number of WiFi Users.", staNum);
  cmd.AddValue("desiredDataRate", "Desired application data rate in kbps.", desiredDataRate);
  cmd.AddValue("packetSize", "Packet size in bytes.", packetSize);
  cmd.AddValue("packetNum", "Number of packets to send.", packetNum);
  cmd.AddValue("debug", "Enable/disable debug log messages to stdout.", debug);
  cmd.AddValue("standard", "WiFi standard [b|g|a|n|ac|ax|ax24|n24]", standard);
  cmd.AddValue("lossExp", "Path loss exponent.", lossExp);
  cmd.AddValue("distances", "Comma separated list of distances.", distancesStr);
  cmd.AddValue("rateControl", "Rate control algorithm [minstrel|minstrelht|constant]. Default is minstrelht.", rateControl);
  cmd.AddValue("phyRate", "Physical rate or \"DataMode\" for constant rate control.", phyRate);
  cmd.AddValue("TxPowerStart", "Start of Tx power range in dBm.", TxPowerStart);
  cmd.AddValue("TxPowerEnd", "End of Tx power range in dBm.", TxPowerEnd);
  cmd.AddValue("TxPowerLevels", "Number of Tx power levels.", TxPowerLevels);
  cmd.AddValue("channelWidth", "Channel width in MHz. Default is 20 MHz.", channelWidth);
  cmd.Parse(argc, argv);

  // This delay is required for the AP to send beacons to the STAs and for the STAs to associate with the AP
  // Application start time is delayed by this amount
  double start_delay = 5.0;
  double simTime = duration + start_delay;

  // Set up logging levels
  if (verbose)
  {
    LogComponentEnable("ee500_WiFi_Sim", LOG_LEVEL_INFO);
  }

  if (debug)
  {
    LogComponentEnable("SenderReceiver", LOG_LEVEL_ALL);
    LogComponentEnable("ee500_WiFi_Sim", LOG_LEVEL_ALL);
  }

  //------------------------------------------------------------
  //-- Create nodes
  //------------------------------------------------------------
  NS_LOG_INFO("Create nodes.");
  NodeContainer nodes;
  nodes.Create(staNum + 1);

  NodeContainer apNodes = NodeContainer(nodes.Get(0));
  NodeContainer staNodes;
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    staNodes.Add(nodes.Get(i));
  }
  NS_LOG_INFO("Number of nodes created: " << nodes.GetN());

  //------------------------------------------------------------
  //-- Setup WiFi
  //------------------------------------------------------------
  NS_LOG_INFO("Setup WiFi.");

  // Set the WiFi standard
  WifiHelper wifi;
  if (standard == "b")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    std::cout << "Standard: 802.11b" << std::endl;
  }
  else if (standard == "a")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
    std::cout << "Standard: 802.11a" << std::endl;
  }
  else if (standard == "g")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    std::cout << "Standard: 802.11g" << std::endl;
  }
  else if (standard == "n")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
    std::cout << "Standard: 802.11n in 5GHZ" << std::endl;
  }
  else if (standard == "n24")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);
    std::cout << "Standard: 802.11n in 2.4GHZ" << std::endl;
  }
  else if (standard == "ac")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);
    std::cout << "Standard: 802.11ac" << std::endl;
  }
  else if (standard == "ax")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ax_5GHZ);
    std::cout << "Standard: 802.11ax in 5GHZ" << std::endl;
  }
  else if (standard == "ax24")
  {
    wifi.SetStandard(WIFI_PHY_STANDARD_80211ax_2_4GHZ);
    std::cout << "Standard: 802.11ax in 2.4GHZ" << std::endl;
  }
  else
  {
    std::cout << "Unknown WiFi standard: " << standard << std::endl;
    exit(1);
  }

  double frequency; // 2.4 or 5.15 GHz
  double c = 3e8;   // Speed of light

  // Set the frequency based on the WiFi standard
  if (standard == "b" || standard == "g" || standard.find("24") != std::string::npos)
  {
    frequency = 2.4e9; // 2.4 GHz
  }
  else
  {
    frequency = 5.15e9; // 5.15 GHz
  }
  double refLoss = 20 * log10(frequency) + 20 * log10(4 * M_PI / c); // Reference loss at 1 meter

  if (verbose)
  {
    // print out the reference loss and the frequency
    std::cout << "Reference loss at 1 meter for " << frequency / 1e9 << " GHz is " << refLoss << " dB" << std::endl;
  }

  // Set the rate control algorithm
  if (rateControl == "minstrel")
  {
    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
    std::cout << "Rate control: Minstrel" << std::endl;
  }
  else if (rateControl == "minstrelht")
  {
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    std::cout << "Rate control: MinstrelHT" << std::endl;
  }
  else if (rateControl == "constant")
  {
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyRate));
    std::cout << "Rate control: Constant"
              << " at " << phyRate << std::endl;
  }
  else
  {
    std::cout << "Unknown rate control algorithm: " << rateControl << std::endl;
    exit(1);
  }

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                 "Exponent", DoubleValue(lossExp),
                                 "ReferenceLoss", DoubleValue(refLoss));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  // Set the transmit power or leave at default if -100
  std::cout << "Number of power levels: " << TxPowerLevels << std::endl;
  if (TxPowerEnd != -100)
  {
    // if TxPowerLevels is 1, then we need to set the TxPowerStart and TxPowerEnd to the same value

    if (TxPowerLevels == 1)
    {
      TxPowerStart = TxPowerEnd;
      wifiPhy.Set("TxPowerStart", DoubleValue(TxPowerStart));
      wifiPhy.Set("TxPowerEnd", DoubleValue(TxPowerEnd));
    }
    else
    {
      wifiPhy.Set("TxPowerStart", DoubleValue(TxPowerStart));
      wifiPhy.Set("TxPowerEnd", DoubleValue(TxPowerEnd));
    }
    std::cout << "TxPowerStart: " << TxPowerStart << " dBm" << std::endl;
    std::cout << "TxPowerEnd: " << TxPowerEnd << " dBm" << std::endl;
  }
  else
  {
    std::cout << "TxPower levels not set, using default" << std::endl;
  }

  // Set the channel width
  if (channelWidth == 20)
  {
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    std::cout << "Channel width: 20 MHz, default" << std::endl;
  }
  else if (channelWidth == 40)
  {
    wifiPhy.Set("ChannelWidth", UintegerValue(40));
    std::cout << "Channel width: 40 MHz" << std::endl;
  }
  else if (channelWidth == 80)
  {
    wifiPhy.Set("ChannelWidth", UintegerValue(80));
    std::cout << "Channel width: 80 MHz" << std::endl;
  }
  else if (channelWidth == 160)
  {
    wifiPhy.Set("ChannelWidth", UintegerValue(160));
    std::cout << "Channel width: 160 MHz" << std::endl;
  }
  else
  {
    std::cout << "Unknown channel width: " << channelWidth << std::endl;
    exit(1);
  }

  Ssid ssid = Ssid("ee500_wifi_sim");
  WifiMacHelper wifiMac;

  // Set up the AP
  wifiMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid),
                  "BeaconGeneration", BooleanValue(true),
                  "BeaconInterval", TimeValue((MicroSeconds(1024000)))); // 1.024 seconds

  NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNodes);

  // Set up the STAs
  wifiMac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  if (verbose)
  {
    // Print out the channel number, frequency, and tx power of each node
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<WifiPhy> phy = nodes.Get(i)->GetDevice(0)->GetObject<WifiNetDevice>()->GetPhy();
      uint16_t channelNumber = phy->GetChannelNumber();
      double frequency = phy->GetFrequency();
      double txPowerStart = phy->GetTxPowerStart();
      double txPowerEnd = phy->GetTxPowerEnd();

      std::cout << "Node " << i << " Channel Number: " << channelNumber
                << " Frequency: " << frequency << " MHz" << std::endl;
      std::cout << "Node " << i << " TxPowerStart: " << txPowerStart << " dBm"
                << " TxPowerEnd: " << txPowerEnd << " dBm" << std::endl;
    }
  }

  if (pcap)
  {
    wifiPhy.EnablePcapAll("all_stations");
  }

  //------------------------------------------------------------
  //-- Setup physical layout
  //------------------------------------------------------------
  NS_LOG_INFO("Create mobility model and place nodes.");
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP

  // Print out the number of APs, STAs and the name of the strategy
  std::cout << "Number of APs: " << apNodes.GetN() << std::endl;
  std::cout << "Number of STAs: " << staNodes.GetN() << std::endl;
  std::cout << "Strategy: " << strategy << std::endl;

  std::vector<double> distances;
  // put default distance in the vector
  distances.push_back(distance);
  if (distancesStr != "")
  {
    std::stringstream ss(distancesStr);
    std::string item;
    std::cout << "Distances (meters): ";
    while (std::getline(ss, item, ','))
    {
      double distance = std::stod(item); // convert string to double
      distances.push_back(distance);
      std::cout << distance << " ";
    }
    std::cout << std::endl;
    std::cout << "Default distance (meters): " << distances[0] << std::endl;
  }
  else
  {
    std::cout << "Distance (meters): " << distance << std::endl;
  }

  if (strategy == "wifi-radial")
  {
    // Place STAs in a circle around the AP
    double theta = 2.0 * M_PI / staNodes.GetN();

    for (uint32_t i = 1; i < nodes.GetN(); ++i) // STAs
    {
      // if distances contains the distance for this STA, use it
      // else use the default distance in element 0
      if (i < distances.size())
      {
        distance = distances[i];
      }
      else
      {
        distance = distances[0];
      }
      double angle = i * theta;
      double x = distance * cos(angle); // x = r * cos(theta)
      double y = distance * sin(angle); // y = r * sin(theta)
      // round to 2 decimal places
      positionAlloc->Add(Vector(round(x * 100) / 100, round(y * 100) / 100, 0.0));
    }
  }
  else
  {
    // Place STAs in a line along the x-axis
    for (uint32_t i = 1; i < nodes.GetN(); ++i) // STAs
    {
      // if distances contains the distance for this STA, use it
      // else use the default distance in element 0
      if (i < distances.size())
      {
        distance = distances[i];
      }
      else
      {
        distance = distances[0];
      }
      positionAlloc->Add(Vector(distance, 0.0, 0.0));
    }
  }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(nodes);

  if (verbose)
  {
    // Print out the positions of the nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
      Ptr<MobilityModel> mobility = nodes.Get(i)->GetObject<MobilityModel>();
      Vector pos = mobility->GetPosition();
      std::cout << "Node " << i << " position: " << pos.x << ", " << pos.y << ", " << pos.z << std::endl;
    }
  }

  //------------------------------------------------------------
  //-- Setup internet stack
  //------------------------------------------------------------
  NS_LOG_INFO("Setup internet stack and assign IP addresses.");
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4Addr;
  ipv4Addr.SetBase("192.168.0.0", "255.255.255.0");

  Ipv4InterfaceContainer apIfaces = ipv4Addr.Assign(apDevice);
  Ipv4InterfaceContainer staIfaces = ipv4Addr.Assign(staDevices);

  if (verbose)
  {
    // Print out the IP addresses of the nodes
    for (uint32_t i = 0; i < apIfaces.GetN(); i++)
    {
      std::cout << "AP Node " << i << " IP: " << apIfaces.GetAddress(i) << std::endl;
    }

    for (uint32_t i = 0; i < staIfaces.GetN(); i++)
    {
      std::cout << "STA Node " << i << " IP: " << staIfaces.GetAddress(i) << std::endl;
    }
  }

  //------------------------------------------------------------
  //-- Create data collector and setup metadata
  //------------------------------------------------------------
  NS_LOG_INFO("Create data collector and setup metadata.");
  DataCollector data;
  data.DescribeRun(experiment, strategy, input, runID);
  data.AddMetadata("distance", std::to_string(distance));
  data.AddMetadata("duration", std::to_string(duration));
  data.AddMetadata("simTime", std::to_string(simTime));
  data.AddMetadata("desiredDataRate", std::to_string(desiredDataRate));
  data.AddMetadata("packetSize", std::to_string(packetSize));
  data.AddMetadata("packetNum", std::to_string(packetNum));
  data.AddMetadata("staNum", std::to_string(staNum));
  data.AddMetadata("standard", standard);
  data.AddMetadata("lossExp", std::to_string(lossExp));
  data.AddMetadata("channelWidth", std::to_string(channelWidth));
  data.AddMetadata("rateControl", rateControl);
  data.AddMetadata("distances", distancesStr);

  if (TxPowerStart != -100 && TxPowerEnd != -100)
  {
    data.AddMetadata("TxPowerStart", std::to_string(TxPowerStart));
    data.AddMetadata("TxPowerEnd", std::to_string(TxPowerEnd));
    data.AddMetadata("TxPowerLevels", std::to_string(TxPowerLevels));
  }
  else
  {
    data.AddMetadata("TxPowerStart", "default");
    data.AddMetadata("TxPowerEnd", "default");
    data.AddMetadata("TxPowerLevels", "default");
  }

  if (rateControl == "constant")
  {
    data.AddMetadata("phyRate", phyRate);
  }
  else
  {
    data.AddMetadata("phyRate", "dynamic");
  }

  //------------------------------------------------------------
  //-- Create traffic between APs and WiFi Users
  //------------------------------------------------------------

  Ptr<Node> apNode = apNodes.Get(0);
  // Iterate over WiFi Users to setup source/sink applications for each AP-User pair

  for (uint32_t i = 0; i < staNodes.GetN(); ++i)
  {
    NS_LOG_INFO("Create traffic source and sink.");

    Ptr<Node> staNode = staNodes.Get(i);
    Ipv4Address dstIpv4Addr = staIfaces.GetAddress(i);

    Ptr<Sender> sender = CreateObject<Sender>();

    double interval = static_cast<double>(packetSize * 8) / (desiredDataRate * 1000); // Calculating packet interval in seconds for desired data rate
    sender->SetAttribute("Interval", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(interval) + "]"));
    sender->SetAttribute("PacketSize", UintegerValue(packetSize)); // bytes
    sender->SetAttribute("NumPackets", UintegerValue(packetNum));
    sender->SetAttribute("Destination", Ipv4AddressValue(dstIpv4Addr)); // Destination address on the WiFi User
    sender->SetAttribute("Port", UintegerValue(1000 + i));              // Listening port on the WiFi User
    apNode->AddApplication(sender);
    sender->SetStartTime(Seconds(start_delay));

    Ptr<Receiver> receiver = CreateObject<Receiver>();
    receiver->SetAttribute("Port", UintegerValue(1000 + i)); // Listening port on the WiFi User
    staNode->AddApplication(receiver);
    receiver->SetStartTime(Seconds(start_delay));

    //------------------------------------------------------------
    //-- Setup stats and data collection of per-user data
    //------------------------------------------------------------
    NS_LOG_INFO("Setup stats and data collection of per-user data.");

    // Create a counter to track how many frames are generated for a given
    // WiFi User.
    Ptr<CounterCalculator<>> appTx =
        CreateObject<CounterCalculator<>>();
    appTx->SetKey("sender-tx-packets");
    appTx->SetContext("node[" + std::to_string(i + 1) + "]");
    sender->SetCounter(appTx);
    data.AddDataCalculator(appTx);

    Ptr<CounterCalculator<>> appRx =
        CreateObject<CounterCalculator<>>();
    appRx->SetKey("receiver-rx-packets");
    appRx->SetContext("node[" + std::to_string(i + 1) + "]");
    receiver->SetCounter(appRx);
    data.AddDataCalculator(appRx);

    Ptr<TimeMinMaxAvgTotalCalculator> delayStat =
        CreateObject<TimeMinMaxAvgTotalCalculator>();
    delayStat->SetKey("delay");
    delayStat->SetContext("node[" + std::to_string(i + 1) + "]");
    receiver->SetDelayTracker(delayStat); // nanoseconds
    data.AddDataCalculator(delayStat);
  }

  //------------------------------------------------------------
  //-- Setup stats and data collection of WiFi Phy data
  //------------------------------------------------------------
  NS_LOG_INFO("Setup stats and data collection of per-station data.");
  WifiStatData wifiStatData;
  wifiStatData.mpduDropCount->SetKey("phy-mpdu-drop-count");
  wifiStatData.mpduDropCount->SetContext("aggregate");
  wifiStatData.mpduDropBytes->SetKey("phy-mpdu-drop-bytes");
  wifiStatData.mpduDropBytes->SetContext("aggregate");
  wifiStatData.mpduRxCount->SetKey("phy-mpdu-rx-count");
  wifiStatData.mpduRxCount->SetContext("aggregate");
  wifiStatData.mpduRxBytes->SetKey("phy-mpdu-rx-bytes");
  wifiStatData.mpduRxBytes->SetContext("aggregate");
  wifiStatData.mpduTxCount->SetKey("phy-mpdu-tx-count");
  wifiStatData.mpduTxCount->SetContext("aggregate");
  wifiStatData.mpduTxBytes->SetKey("phy-mpdu-tx-bytes");
  wifiStatData.mpduTxBytes->SetContext("aggregate");
  wifiStatData.mpduRxRSSsum->SetKey("phy-mpdu-rx-rss-sum");
  wifiStatData.mpduRxRSSsum->SetContext("aggregate");
  data.AddDataCalculator(wifiStatData.mpduDropCount);
  data.AddDataCalculator(wifiStatData.mpduDropBytes);
  data.AddDataCalculator(wifiStatData.mpduRxCount);
  data.AddDataCalculator(wifiStatData.mpduRxBytes);
  data.AddDataCalculator(wifiStatData.mpduTxCount);
  data.AddDataCalculator(wifiStatData.mpduTxBytes);
  data.AddDataCalculator(wifiStatData.mpduRxRSSsum);

  // Iterate over staDevices to setup stats and data collection of per-station data
  for (uint32_t i = 0; i < staDevices.GetN(); ++i)
  {
    // GetPhy() returns the WifiPhy object for the NetDevice
    Ptr<WifiNetDevice> wifiDevice = staDevices.Get(i)->GetObject<WifiNetDevice>();
    Ptr<WifiPhy> phy = wifiDevice->GetPhy();
    Mac48Address macAddress = Mac48Address::ConvertFrom(wifiDevice->GetAddress());
    phy->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDropCallback, macAddress, &wifiStatData));
    phy->TraceConnectWithoutContext("MonitorSnifferRx", MakeBoundCallback(&MonitorSniffRxCallback, macAddress, &wifiStatData));
  }

  //------------------------------------------------------------
  //-- Setup aggregate stats and data collection
  //------------------------------------------------------------
  NS_LOG_INFO("Setup aggregate stats and data collection.");

  // Create a counter to track how many frames are generated on the AP.
  // Updates are triggered by the trace signal generated by the WiFi MAC model
  // object.
  Ptr<PacketCounterCalculator> totalMacTx =
      CreateObject<PacketCounterCalculator>();
  totalMacTx->SetKey("mac-tx-frames");
  totalMacTx->SetContext("aggregate");
  Config::Connect("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate, totalMacTx));
  data.AddDataCalculator(totalMacTx);

  // Create a counter to track how many frames are received on STAs.
  // Updates are triggered by the trace signal generated by the WiFi MAC model
  // object.
  Ptr<PacketCounterCalculator> totalMacRx =
      CreateObject<PacketCounterCalculator>();
  totalMacRx->SetKey("mac-rx-frames");
  totalMacRx->SetContext("aggregate");
  for (uint32_t i = 1; i < nodes.GetN(); i++)
  {
    Config::Connect("/NodeList/" + std::to_string(i) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                    MakeCallback(&PacketCounterCalculator::PacketUpdate,
                                 totalMacRx));
  }
  data.AddDataCalculator(totalMacRx);

  // This counter tracks how many packets are sent by the Senders
  // to the Receivers.  It's connected to the trace signal generated
  // by the Sender Application.
  Ptr<PacketCounterCalculator> totalAppTx =
      CreateObject<PacketCounterCalculator>();
  totalAppTx->SetKey("sender-tx-packets");
  totalAppTx->SetContext("aggregate");
  Config::Connect("/NodeList/0/ApplicationList/*/$Sender/Tx",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate,
                               totalAppTx));
  data.AddDataCalculator(totalAppTx);

  // This counter tracks how many packets are received by the Receivers.
  // It's connected to the trace signal generated
  // by the Receiver Application.
  Ptr<PacketCounterCalculator> totalAppRx =
      CreateObject<PacketCounterCalculator>();
  totalAppRx->SetKey("receiver-rx-packets");
  totalAppRx->SetContext("aggregate");
  Config::Connect("/NodeList/*/ApplicationList/*/$Receiver/Rx",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate,
                               totalAppRx));
  data.AddDataCalculator(totalAppRx);

  //------------------------------------------------------------
  //-- Run the simulation
  //------------------------------------------------------------
  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  //------------------------------------------------------------
  //-- Generate statistics output.
  //------------------------------------------------------------

  Ptr<DataOutputInterface> output = CreateObject<SqliteDataOutput>();
  Ptr<LocalDataOutput> output_local = CreateObject<LocalDataOutput>();

  // Take the data from DataCollector and output it to SQLight file and local object
  if (output != 0)
  {
    output->SetFilePrefix("data");
    output->Output(data);
  }
  output_local->Output(data);

  std::map<std::string, std::string> metadata = output_local->GetMetadata();
  std::cout << std::endl;
  std::cout << std::left << std::setw(60) << "Metadata" << std::setw(20) << "Value" << std::endl;
  std::cout << std::setfill('-') << std::setw(80) << "-" << std::endl; // Print line
  std::cout << std::setfill(' ');                                      // Reset fill character
  for (auto &data : metadata)
  {
    std::cout << std::setw(60) << data.first << std::setw(20) << data.second << std::endl;
  }

  std::map<std::string, double> counters = output_local->GetCounters();
  std::cout << std::endl;
  std::cout << std::left << std::setw(60) << "Counter" << std::setw(20) << "Value" << std::endl;
  std::cout << std::setfill('-') << std::setw(80) << "-" << std::endl; // Print line
  std::cout << std::setfill(' ');                                      // Reset fill character
  for (auto &data : counters)
  {
    // all the values are double, but some has no decimal places
    // if the value has decimal place, then print it with 8 decimal places
    // otherwise, print it with no decimal places
    std::stringstream stream;
    if (data.second - (int)data.second > 0)
    {
      stream << std::fixed << std::setprecision(8) << data.second;
    }
    else
    {
      stream << std::fixed << std::setprecision(0) << data.second;
    }
    std::cout << std::setw(60) << data.first << std::setw(20) << stream.str() << std::endl;
  }

  double appDataTXRate = (double)totalAppTx->GetCount() * packetSize * 8.0 / (double)duration / 1000.0;
  double appDataRXRate = (double)totalAppRx->GetCount() * packetSize * 8.0 / (double)duration / 1000.0;
  double appDataLossRatio = (double)(totalAppTx->GetCount() - totalAppRx->GetCount()) / (double)totalAppTx->GetCount();

  // if delay-average_node in the key name, then sum it into the total delay. Divide by the number of nodes later
  double totalDelaySum = 0.0;
  uint32_t totalDelayCount = 0;
  for (uint32_t i = 0; i < staDevices.GetN(); i++)
  {
    std::string key = "delay-average_node[" + std::to_string(i + 1) + "]";
    if (counters.find(key) != counters.end())
    {
      totalDelaySum += counters[key];
      totalDelayCount++;
    }
  }
  // if totalDelayCount is zero, then set avgDelay to zero
  double appAvgDelay = totalDelayCount > 0 ? totalDelaySum / (double)totalDelayCount * 1000 : 0.0; // Convert to ms

  // Payload size presented to MAC layer is APP_SIZE + UDP_HEADER_SIZE + IP_HEADER_SIZE
  uint16_t macPayloadSize = packetSize + 8 + 20;
  // if totalMacTx->GetCount() - totalMacRx->GetCount() < 0 then macDropCount = 0
  int64_t totalMacLoss = ((int64_t)totalMacTx->GetCount() - (int64_t)totalMacRx->GetCount()) > 0 ? ((int64_t)totalMacTx->GetCount() - (int64_t)totalMacRx->GetCount()) : 0;

  double macDataTXRate = totalMacTx->GetCount() * macPayloadSize * 8 / (double)duration / 1000.0;
  double macDataRXRate = totalMacRx->GetCount() * macPayloadSize * 8 / (double)duration / 1000.0;
  double macDataLossRatio = (double)totalMacLoss / (double)totalMacTx->GetCount();

  uint64_t wifiDataTXCount = wifiStatData.mpduTxCount->GetCount();
  double avgRSS = wifiStatData.mpduRxRSSsum->GetCount() / wifiStatData.mpduRxCount->GetCount();

  double wifiDataTXRate = (double)wifiStatData.mpduTxBytes->GetCount() * 8.0 / (double)duration / 1000.0;
  double wifiDataRXRate = (double)wifiStatData.mpduRxBytes->GetCount() * 8.0 / (double)duration / 1000.0;
  double wifiDataLossRatio = (double)wifiStatData.mpduDropCount->GetCount() / (double)wifiDataTXCount;

  // Print table header
  std::cout << std::endl;
  std::cout << std::left << std::setw(60) << "Metric" << std::setw(20) << "Value" << std::endl;
  std::cout << std::setfill('-') << std::setw(80) << "-" << std::endl; // Print line
  std::cout << std::setfill(' ');                                      // Reset fill character

  // Print data
  std::cout << std::setw(60) << "[App] Offered Load or App Data TX Rate (kbps):" << std::setw(20) << appDataTXRate << std::endl;
  std::cout << std::setw(60) << "[App] Throughput or App Data RX Rate (kbps):" << std::setw(20) << appDataRXRate << std::endl;
  std::cout << std::setw(60) << "[App] Loss Ratio:" << std::setw(20) << appDataLossRatio << std::endl;
  std::cout << std::setw(60) << "[App] Average Delay (ms):" << std::setw(20) << appAvgDelay << std::endl;
  std::cout << std::setw(60) << "[MAC] MAC Data TX Rate (kbps):" << std::setw(20) << macDataTXRate << std::endl;
  std::cout << std::setw(60) << "[MAC] MAC Data RX Rate (kbps):" << std::setw(20) << macDataRXRate << std::endl;
  std::cout << std::setw(60) << "[MAC] MAC Data Loss Ratio:" << std::setw(20) << macDataLossRatio << std::endl;
  std::cout << std::setw(60) << "[Phy] WiFi Data TX Rate (kbps):" << std::setw(20) << wifiDataTXRate << std::endl;
  std::cout << std::setw(60) << "[Phy] WiFi Data RX Rate (kbps):" << std::setw(20) << wifiDataRXRate << std::endl;
  std::cout << std::setw(60) << "[Phy] WiFi Data Loss Ratio:" << std::setw(20) << wifiDataLossRatio << std::endl;
  std::cout << std::setw(60) << "[Phy] Average RSS (dBm):" << std::setw(20) << avgRSS << std::endl;

  // Free any memory here at the end of this example.
  Simulator::Destroy();
}
