/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/internet-module.h>
#include <ns3/lte-module.h>
#include <ns3/config-store-module.h>
#include <ns3/buildings-module.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/applications-module.h>
#include <ns3/log.h>
#include <iomanip>
#include <ios>
#include <string>
#include <vector>

// The topology of this simulation program is inspired from 
// 3GPP R4-092042, Section 4.2.1 Dual Stripe Model
// note that the term "apartments" used in that document matches with
// the term "room" used in the BuildingsMobilityModel 

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LenaDualStripe");

bool AreOverlapping (Box a, Box b)
{
  return !((a.xMin > b.xMax) || (b.xMin > a.xMax) || (a.yMin > b.yMax) || (b.yMin > a.yMax));
}

/**
 * 用于在给定区域内安装建筑块的类。建筑物按照双条纹场景的方式安装。
 */
class FemtocellBlockAllocator
{
public:
    
  /**
   * 构造函数
   * \param area 总面积
   * \param nApartmentsX X方向上的公寓数量
   * \param nFloors 楼层数量
   */
  FemtocellBlockAllocator (Box area, uint32_t nApartmentsX, uint32_t nFloors);


   /**
   * 创建建筑块的函数。
   * \param n 要创建的建筑块数量
   */
  void Create (uint32_t n);
  /// Create function
  void Create ();

private:
  /**
   * 检查盒子区域是否与先前创建的某些建筑块重叠的函数。
   * \param box 要检查的区域
   * \returns 如果存在重叠则返回true
   */
  bool OverlapsWithAnyPrevious (Box box);
  Box m_area; ///< Area
  uint32_t m_nApartmentsX; ///< X apartments 
  uint32_t m_nFloors; ///< number of floors
  std::list<Box> m_previousBlocks; ///< previous bocks
  double m_xSize; ///< X size
  double m_ySize; ///< Y size
  Ptr<UniformRandomVariable> m_xMinVar; ///< X minimum variance
  Ptr<UniformRandomVariable> m_yMinVar; ///< Y minimum variance

};
/*这个构造函数首先使用成员初始化列表初始化了类的成员变量，包括总面积、X方向上的公寓数量、楼层数量等。
接下来，它计算了X和Y方向上的建筑块尺寸（m_xSize和m_ySize）。
然后，它为X和Y方向上的最小变量创建了均匀随机变量，并设置了相应的属性，确保新建筑块的位置不会超出给定的区域范围。*/
FemtocellBlockAllocator::FemtocellBlockAllocator (Box area, uint32_t nApartmentsX, uint32_t nFloors)
  : m_area (area),
    m_nApartmentsX (nApartmentsX),
    m_nFloors (nFloors),
    m_xSize (nApartmentsX*10 + 50),
    m_ySize (170)
{
  m_xMinVar = CreateObject<UniformRandomVariable> ();
  m_xMinVar->SetAttribute ("Min", DoubleValue (area.xMin));
  m_xMinVar->SetAttribute ("Max", DoubleValue (area.xMax - m_xSize));
  m_yMinVar = CreateObject<UniformRandomVariable> ();
  m_yMinVar->SetAttribute ("Min", DoubleValue (area.yMin));
  m_yMinVar->SetAttribute ("Max", DoubleValue (area.yMax - m_ySize));
}

void 
FemtocellBlockAllocator::Create (uint32_t n)
{
  for (uint32_t i = 0; i < n; ++i)
    {
      Create ();
    }
}

void
FemtocellBlockAllocator::Create ()
{
  Box box;
  uint32_t attempt = 0;
  do 
    {
      NS_ASSERT_MSG (attempt < 100, "Too many failed attempts to position apartment block. Too many blocks? Too small area?");
      box.xMin = m_xMinVar->GetValue ();
      box.xMax = box.xMin + m_xSize;
      box.yMin = m_yMinVar->GetValue ();
      box.yMax = box.yMin + m_ySize;
      ++attempt;
    }
  while (OverlapsWithAnyPrevious (box));

  NS_LOG_LOGIC ("allocated non overlapping block " << box);
  m_previousBlocks.push_back (box);
  Ptr<GridBuildingAllocator>  gridBuildingAllocator;
  gridBuildingAllocator = CreateObject<GridBuildingAllocator> ();
  gridBuildingAllocator->SetAttribute ("GridWidth", UintegerValue (1));
  gridBuildingAllocator->SetAttribute ("LengthX", DoubleValue (10*m_nApartmentsX)); 
  gridBuildingAllocator->SetAttribute ("LengthY", DoubleValue (20*2));
  gridBuildingAllocator->SetAttribute ("DeltaX", DoubleValue (10));
  gridBuildingAllocator->SetAttribute ("DeltaY", DoubleValue (10));
  gridBuildingAllocator->SetAttribute ("Height", DoubleValue (3*m_nFloors));
  gridBuildingAllocator->SetBuildingAttribute ("NRoomsX", UintegerValue (m_nApartmentsX));
  gridBuildingAllocator->SetBuildingAttribute ("NRoomsY", UintegerValue (2));
  gridBuildingAllocator->SetBuildingAttribute ("NFloors", UintegerValue (m_nFloors));
  gridBuildingAllocator->SetAttribute ("MinX", DoubleValue (box.xMin + 10));
  gridBuildingAllocator->SetAttribute ("MinY", DoubleValue (box.yMin + 10));
  gridBuildingAllocator->Create (2);
}

bool 
FemtocellBlockAllocator::OverlapsWithAnyPrevious (Box box)
{
  for (std::list<Box>::iterator it = m_previousBlocks.begin (); it != m_previousBlocks.end (); ++it)
    {
      if (AreOverlapping (*it, box))
        {
          return true;
        }
    }
  return false;
}

void 
PrintGnuplottableBuildingListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  uint32_t index = 0;
  for (BuildingList::Iterator it = BuildingList::Begin (); it != BuildingList::End (); ++it)
    {
      ++index;
      Box box = (*it)->GetBoundaries ();
      outFile << "set object " << index
              << " rect from " << box.xMin  << "," << box.yMin
              << " to "   << box.xMax  << "," << box.yMax
              << " front fs empty "
              << std::endl;
    }
}

void 
PrintGnuplottableUeListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  for (NodeList::Iterator it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
      Ptr<Node> node = *it;
      int nDevs = node->GetNDevices ();
      for (int j = 0; j < nDevs; j++)
        {
          Ptr<LteUeNetDevice> uedev = node->GetDevice (j)->GetObject <LteUeNetDevice> ();
          if (uedev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              outFile << "set label \"" << uedev->GetImsi ()
                      << "\" at "<< pos.x << "," << pos.y << " left font \"Helvetica,4\" textcolor rgb \"grey\" front point pt 1 ps 0.3 lc rgb \"grey\" offset 0,0"
                      << std::endl;
            }
        }
    }
}       //这个函数的主要作用是将LTE UE（用户）的IMSI标签信息写入文件，以便后续在Gnuplot中绘制。每个用户是图中的一个灰色点

void 
PrintGnuplottableEnbListToFile (std::string filename)
{
  std::ofstream outFile;
  outFile.open (filename.c_str (), std::ios_base::out | std::ios_base::trunc);
  if (!outFile.is_open ())
    {
      NS_LOG_ERROR ("Can't open file " << filename);
      return;
    }
  for (NodeList::Iterator it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
      Ptr<Node> node = *it;
      int nDevs = node->GetNDevices ();
      for (int j = 0; j < nDevs; j++)
        {
          Ptr<LteEnbNetDevice> enbdev = node->GetDevice (j)->GetObject <LteEnbNetDevice> ();
          if (enbdev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              outFile << "set label \"" << enbdev->GetCellId ()
                      << "\" at "<< pos.x << "," << pos.y
                      << " left font \"Helvetica,4\" textcolor rgb \"white\" front  point pt 2 ps 0.3 lc rgb \"white\" offset 0,0"
                      << std::endl;
            }
        }
    }
}  //这个函数与之前的PrintGnuplottableUeListToFile函数相似，它的目的是将LTE eNB（基站）的CellId作为标签写入文件，以便后续在Gnuplot中绘制。白色点为基站


static ns3::GlobalValue g_nBlocks ("nBlocks",
                                   "Number of femtocell blocks",
                                   ns3::UintegerValue (4),
                                   ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nApartmentsX ("nApartmentsX",
                                        "Number of apartments along the X axis in a femtocell block",
                                        ns3::UintegerValue (10),
                                        ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nFloors ("nFloors",
                                   "Number of floors",
                                   ns3::UintegerValue (1),
                                   ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nMacroEnbSites ("nMacroEnbSites",
                                          "How many macro sites there are",
                                          ns3::UintegerValue (3),
                                          ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_nMacroEnbSitesX ("nMacroEnbSitesX",
                                           "(minimum) number of sites along the X-axis of the hex grid",
                                           ns3::UintegerValue (1),
                                           ns3::MakeUintegerChecker<uint32_t> ());
static ns3::GlobalValue g_interSiteDistance ("interSiteDistance",
                                             "min distance between two nearby macro cell sites",
                                             ns3::DoubleValue (500),
                                             ns3::MakeDoubleChecker<double> ());            //基站距离
static ns3::GlobalValue g_areaMarginFactor ("areaMarginFactor",
                                            "how much the UE area extends outside the macrocell grid, "
                                            "expressed as fraction of the interSiteDistance",
                                            ns3::DoubleValue (0.5),
                                            ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_macroUeDensity ("macroUeDensity",
                                          "How many macrocell UEs there are per square meter",
                                          ns3::DoubleValue (0.00002),
                                          ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeEnbDeploymentRatio ("homeEnbDeploymentRatio",
                                                  "The HeNB deployment ratio as per 3GPP R4-092042",
                                                  ns3::DoubleValue (0.2),
                                                  ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeEnbActivationRatio ("homeEnbActivationRatio",
                                                  "The HeNB activation ratio as per 3GPP R4-092042",
                                                  ns3::DoubleValue (0.5),
                                                  ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeUesHomeEnbRatio ("homeUesHomeEnbRatio",
                                               "How many (on average) home UEs per HeNB there are in the simulation",
                                               ns3::DoubleValue (1.0),
                                               ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_macroEnbTxPowerDbm ("macroEnbTxPowerDbm",                    //基站发送功率
                                              "TX power [dBm] used by macro eNBs",
                                              ns3::DoubleValue (46.0),
                                              ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_homeEnbTxPowerDbm ("homeEnbTxPowerDbm",
                                             "TX power [dBm] used by HeNBs",
                                             ns3::DoubleValue (20.0),
                                             ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_macroEnbDlEarfcn ("macroEnbDlEarfcn",
                                            "DL EARFCN used by macro eNBs",
                                            ns3::UintegerValue (100),
                                            ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_homeEnbDlEarfcn ("homeEnbDlEarfcn",
                                           "DL EARFCN used by HeNBs",
                                           ns3::UintegerValue (100),
                                           ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_macroEnbBandwidth ("macroEnbBandwidth",
                                             "bandwidth [num RBs] used by macro eNBs",
                                             ns3::UintegerValue (25),
                                             ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_homeEnbBandwidth ("homeEnbBandwidth",
                                            "bandwidth [num RBs] used by HeNBs",
                                            ns3::UintegerValue (25),       //系统带宽
                                            ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_simTime ("simTime",
                                   "Total duration of the simulation [s]",
                                   ns3::DoubleValue (0.25),
                                   ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_generateRem ("generateRem",
                                       "if true, will generate a REM and then abort the simulation;"
                                       "if false, will run the simulation normally (without generating any REM)",
                                       ns3::BooleanValue (false),
                                       ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_remRbId ("remRbId",
                                   "Resource Block Id of Data Channel, for which REM will be generated;"
                                   "default value is -1, what means REM will be averaged from all RBs of "
                                   "Control Channel",
                                   ns3::IntegerValue (-1),
                                   MakeIntegerChecker<int32_t> ());
static ns3::GlobalValue g_epc ("epc",
                               "If true, will setup the EPC to simulate an end-to-end topology, "
                               "with real IP applications over PDCP and RLC UM (or RLC AM by changing "
                               "the default value of EpsBearerToRlcMapping e.g. to RLC_AM_ALWAYS). "
                               "If false, only the LTE radio access will be simulated with RLC SM. ",
                               ns3::BooleanValue (false),
                               ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_epcDl ("epcDl",
                                 "if true, will activate data flows in the downlink when EPC is being used. "
                                 "If false, downlink flows won't be activated. "
                                 "If EPC is not used, this parameter will be ignored.",
                                 ns3::BooleanValue (true),
                                 ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_epcUl ("epcUl",
                                 "if true, will activate data flows in the uplink when EPC is being used. "
                                 "If false, uplink flows won't be activated. "
                                 "If EPC is not used, this parameter will be ignored.",
                                 ns3::BooleanValue (true),
                                 ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_useUdp ("useUdp",
                                  "if true, the UdpClient application will be used. "
                                  "Otherwise, the BulkSend application will be used over a TCP connection. "
                                  "If EPC is not used, this parameter will be ignored.",
                                  ns3::BooleanValue (true),
                                  ns3::MakeBooleanChecker ());
static ns3::GlobalValue g_fadingTrace ("fadingTrace",
                                       "The path of the fading trace (by default no fading trace "
                                       "is loaded, i.e., fading is not considered)",
                                       ns3::StringValue (""),
                                       ns3::MakeStringChecker ());
static ns3::GlobalValue g_numBearersPerUe ("numBearersPerUe",
                                           "How many bearers per UE there are in the simulation",
                                           ns3::UintegerValue (1),
                                           ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_srsPeriodicity ("srsPeriodicity",
                                          "SRS Periodicity (has to be at least "
                                          "greater than the number of UEs per eNB)",
                                          ns3::UintegerValue (80),
                                          ns3::MakeUintegerChecker<uint16_t> ());
static ns3::GlobalValue g_outdoorUeMinSpeed ("outdoorUeMinSpeed",
                                             "Minimum speed value of macro UE with random waypoint model [m/s].",
                                             ns3::DoubleValue (0.0),
                                             ns3::MakeDoubleChecker<double> ());
static ns3::GlobalValue g_outdoorUeMaxSpeed ("outdoorUeMaxSpeed",
                                             "Maximum speed value of macro UE with random waypoint model [m/s].",
                                             ns3::DoubleValue (0.0),
                                             ns3::MakeDoubleChecker<double> ());

int
main (int argc, char *argv[])
{
  // change some default attributes so that they are reasonable for
  // this scenario, but do this before processing command line
  // arguments, so that the user is allowed to override these settings 
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (1)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000000));
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (10 * 1024));

  CommandLine cmd;
  cmd.Parse (argc, argv);
  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();
  // parse again so you can override input file default values via command line
  cmd.Parse (argc, argv); 

  // the scenario parameters get their values from the global attributes defined above
  UintegerValue uintegerValue;
  IntegerValue integerValue;
  DoubleValue doubleValue;
  BooleanValue booleanValue;
  StringValue stringValue;
  GlobalValue::GetValueByName ("nBlocks", uintegerValue);
  uint32_t nBlocks = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nApartmentsX", uintegerValue);
  uint32_t nApartmentsX = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nFloors", uintegerValue);
  uint32_t nFloors = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nMacroEnbSites", uintegerValue);
  uint32_t nMacroEnbSites = uintegerValue.Get ();
  GlobalValue::GetValueByName ("nMacroEnbSitesX", uintegerValue);
  uint32_t nMacroEnbSitesX = uintegerValue.Get ();
  GlobalValue::GetValueByName ("interSiteDistance", doubleValue);     //cmd设置基站距离
  double interSiteDistance = doubleValue.Get ();
  GlobalValue::GetValueByName ("areaMarginFactor", doubleValue);
  double areaMarginFactor = doubleValue.Get ();
  GlobalValue::GetValueByName ("macroUeDensity", doubleValue);
  double macroUeDensity = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeEnbDeploymentRatio", doubleValue);
  double homeEnbDeploymentRatio = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeEnbActivationRatio", doubleValue);
  double homeEnbActivationRatio = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeUesHomeEnbRatio", doubleValue);
  double homeUesHomeEnbRatio = doubleValue.Get ();
  GlobalValue::GetValueByName ("macroEnbTxPowerDbm", doubleValue);
  double macroEnbTxPowerDbm = doubleValue.Get ();
  GlobalValue::GetValueByName ("homeEnbTxPowerDbm", doubleValue);
  double homeEnbTxPowerDbm = doubleValue.Get ();
  GlobalValue::GetValueByName ("macroEnbDlEarfcn", uintegerValue);
  uint32_t macroEnbDlEarfcn = uintegerValue.Get ();
  GlobalValue::GetValueByName ("homeEnbDlEarfcn", uintegerValue);
  uint32_t homeEnbDlEarfcn = uintegerValue.Get ();
  GlobalValue::GetValueByName ("macroEnbBandwidth", uintegerValue);
  uint16_t macroEnbBandwidth = uintegerValue.Get ();
  GlobalValue::GetValueByName ("homeEnbBandwidth", uintegerValue);
  uint16_t homeEnbBandwidth = uintegerValue.Get ();
  GlobalValue::GetValueByName ("simTime", doubleValue);
  double simTime = doubleValue.Get ();
  GlobalValue::GetValueByName ("epc", booleanValue);
  bool epc = booleanValue.Get ();
  GlobalValue::GetValueByName ("epcDl", booleanValue);
  bool epcDl = booleanValue.Get ();
  GlobalValue::GetValueByName ("epcUl", booleanValue);
  bool epcUl = booleanValue.Get ();
  GlobalValue::GetValueByName ("useUdp", booleanValue);
  bool useUdp = booleanValue.Get ();
  GlobalValue::GetValueByName ("generateRem", booleanValue);
  bool generateRem = booleanValue.Get ();
  GlobalValue::GetValueByName ("remRbId", integerValue);
  int32_t remRbId = integerValue.Get ();
  GlobalValue::GetValueByName ("fadingTrace", stringValue);
  std::string fadingTrace = stringValue.Get ();
  GlobalValue::GetValueByName ("numBearersPerUe", uintegerValue);
  uint16_t numBearersPerUe = uintegerValue.Get ();
  GlobalValue::GetValueByName ("srsPeriodicity", uintegerValue);
  uint16_t srsPeriodicity = uintegerValue.Get ();
  GlobalValue::GetValueByName ("outdoorUeMinSpeed", doubleValue);
  uint16_t outdoorUeMinSpeed = doubleValue.Get ();
  GlobalValue::GetValueByName ("outdoorUeMaxSpeed", doubleValue);
  uint16_t outdoorUeMaxSpeed = doubleValue.Get ();

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (srsPeriodicity));

  Box macroUeBox; // 定义一个 Box 对象，用于表示 UE 的位置范围
  double ueZ = 1.5; //UE的Z轴坐标
  if (nMacroEnbSites > 0)// 如果存在宏基站
    { 
      uint32_t currentSite = nMacroEnbSites -1;
      uint32_t biRowIndex = (currentSite / (nMacroEnbSitesX + nMacroEnbSitesX + 1));
      uint32_t biRowRemainder = currentSite % (nMacroEnbSitesX + nMacroEnbSitesX + 1);
      uint32_t rowIndex = biRowIndex*2 + 1;
      if (biRowRemainder >= nMacroEnbSitesX)
        {
          ++rowIndex;
        }
      uint32_t nMacroEnbSitesY = rowIndex;
      NS_LOG_LOGIC ("nMacroEnbSitesY = " << nMacroEnbSitesY);
                // 根据基站的位置信息计算 UE 的位置范围
      macroUeBox = Box (-areaMarginFactor*interSiteDistance, 
                        (nMacroEnbSitesX + areaMarginFactor)*interSiteDistance, 
                        -areaMarginFactor*interSiteDistance, 
                        (nMacroEnbSitesY -1)*interSiteDistance*sqrt (0.75) + areaMarginFactor*interSiteDistance,
                        ueZ, ueZ);
    }
  else
    {         // 如果不存在宏基站，设置默认的 UE 位置范围
      // still need the box to place femtocell blocks
      macroUeBox = Box (0, 150, 0, 150, ueZ, ueZ);
    }

  FemtocellBlockAllocator blockAllocator (macroUeBox, nApartmentsX, nFloors);
  blockAllocator.Create (nBlocks);
    /*这段代码的目的似乎是为 UE 创建位置信息。在存在宏基站的情况下，通过计算基站的位置信息来确定 UE 的位置范围；而在没有宏基站的情况下，
    使用默认的位置范围。最后，通过 FemtocellBlockAllocator 对象创建分区块，可能是用于模拟网络中的小区块或其他实体。*/

  uint32_t nHomeEnbs = round (4 * nApartmentsX * nBlocks * nFloors * homeEnbDeploymentRatio * homeEnbActivationRatio);
  NS_LOG_LOGIC ("nHomeEnbs = " << nHomeEnbs);
  uint32_t nHomeUes = round (nHomeEnbs * homeUesHomeEnbRatio);
  NS_LOG_LOGIC ("nHomeUes = " << nHomeUes);
  double macroUeAreaSize = (macroUeBox.xMax - macroUeBox.xMin) * (macroUeBox.yMax - macroUeBox.yMin);
  uint32_t nMacroUes = round (macroUeAreaSize * macroUeDensity);
  NS_LOG_LOGIC ("nMacroUes = " << nMacroUes << " (density=" << macroUeDensity << ")");

  NodeContainer homeEnbs;
  homeEnbs.Create (nHomeEnbs);
  NodeContainer macroEnbs;
  macroEnbs.Create (3 * nMacroEnbSites);
  NodeContainer homeUes;
  homeUes.Create (nHomeUes);
  NodeContainer macroUes;
  macroUes.Create (nMacroUes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");


  Ptr <LteHelper> lteHelper = CreateObject<LteHelper> ();
  lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::HybridBuildingsPropagationLossModel"));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaExtWalls", DoubleValue (0));
  lteHelper->SetAttribute ("PathlossModel", StringValue("ns3::HybridBuildingsPropagationLossModel"));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaOutdoor", DoubleValue (1));
  lteHelper->SetPathlossModelAttribute ("ShadowSigmaIndoor", DoubleValue (1.5));
  // use always LOS model
  lteHelper->SetPathlossModelAttribute ("Los2NlosThr", DoubleValue (1e6));
  lteHelper->SetSpectrumChannelType ("ns3::MultiModelSpectrumChannel");
  

//   lteHelper->EnableLogComponents ();
//   LogComponentEnable ("PfFfMacScheduler", LOG_LEVEL_ALL);

  if (!fadingTrace.empty ())
    {
      lteHelper->SetAttribute ("FadingModel", StringValue ("ns3::TraceFadingLossModel"));
      lteHelper->SetFadingModelAttribute ("TraceFilename", StringValue (fadingTrace));
    }

  Ptr<PointToPointEpcHelper> epcHelper;
  if (epc)
    {
      NS_LOG_LOGIC ("enabling EPC");
      epcHelper = CreateObject<PointToPointEpcHelper> ();
      lteHelper->SetEpcHelper (epcHelper);
    }

  // Macro eNBs in 3-sector hex grid      ****************  基站设置 *************************
  // 安装宏基站的移动性
  mobility.Install (macroEnbs);
  // 在建筑物帮助器中安装宏基站
  BuildingsHelper::Install (macroEnbs);
  // 创建 LTE Hex Grid 基站拓扑助手
  Ptr<LteHexGridEnbTopologyHelper> lteHexGridEnbTopologyHelper = CreateObject<LteHexGridEnbTopologyHelper> ();
  lteHexGridEnbTopologyHelper->SetLteHelper (lteHelper);
  lteHexGridEnbTopologyHelper->SetAttribute ("InterSiteDistance", DoubleValue (interSiteDistance));
  lteHexGridEnbTopologyHelper->SetAttribute ("MinX", DoubleValue (interSiteDistance/2));
  lteHexGridEnbTopologyHelper->SetAttribute ("GridWidth", UintegerValue (nMacroEnbSitesX));
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (macroEnbTxPowerDbm));
  // 设置宏基站的物理层参数
  lteHelper->SetEnbAntennaModelType ("ns3::ParabolicAntennaModel");
  lteHelper->SetEnbAntennaModelAttribute ("Beamwidth",   DoubleValue (70));      //天线波束宽度
  lteHelper->SetEnbAntennaModelAttribute ("MaxAttenuation",     DoubleValue (25.0));//天线最大衰减
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (macroEnbDlEarfcn));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (macroEnbDlEarfcn + 18000));
  lteHelper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (macroEnbBandwidth));
  lteHelper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (macroEnbBandwidth));
  
  // 使用 Hex Grid Enb Topology 助手设置宏基站的位置并安装设备
  NetDeviceContainer macroEnbDevs = lteHexGridEnbTopologyHelper->SetPositionAndInstallEnbDevice (macroEnbs);

  if (epc)
    {
      // 启用宏基站的 X2 接口，支持切换
      lteHelper->AddX2Interface (macroEnbs);
    }
  
  // HomeEnbs randomly indoor         设置家庭基站和相应的用户设备的移动性和网络属性
// 设置家庭基站的随机室内位置
  Ptr<PositionAllocator> positionAlloc = CreateObject<RandomRoomPositionAllocator> ();
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (homeEnbs);
  BuildingsHelper::Install (homeEnbs);
  // 针对家庭基站设置物理层参数
  Config::SetDefault ("ns3::LteEnbPhy::TxPower", DoubleValue (homeEnbTxPowerDbm));
  lteHelper->SetEnbAntennaModelType ("ns3::IsotropicAntennaModel");
  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (homeEnbDlEarfcn));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (homeEnbDlEarfcn + 18000));
  lteHelper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (homeEnbBandwidth));
  lteHelper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (homeEnbBandwidth));
  lteHelper->SetEnbDeviceAttribute ("CsgId", UintegerValue (1));
  lteHelper->SetEnbDeviceAttribute ("CsgIndication", BooleanValue (true));
  NetDeviceContainer homeEnbDevs  = lteHelper->InstallEnbDevice (homeEnbs);

  // home UEs located in the same apartment in which there are the Home eNBs
  // 家庭UE位于与家庭基站相同的公寓中
  positionAlloc = CreateObject<SameRoomPositionAllocator> (homeEnbs);
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (homeUes);
  BuildingsHelper::Install (homeUes);
  // set the home UE as a CSG member of the home eNodeBs
  // 将家庭UE设置为家庭eNodeBs的CSG成员
  lteHelper->SetUeDeviceAttribute ("CsgId", UintegerValue (1));
  NetDeviceContainer homeUeDevs = lteHelper->InstallUeDevice (homeUes);

  // macro Ues
  NS_LOG_LOGIC ("randomly allocating macro UEs in " << macroUeBox << " speedMin " << outdoorUeMinSpeed << " speedMax " << outdoorUeMaxSpeed);
  if (outdoorUeMaxSpeed!=0.0)
    {
      mobility.SetMobilityModel ("ns3::SteadyStateRandomWaypointMobilityModel");
      
      Config::SetDefault ("ns3::SteadyStateRandomWaypointMobilityModel::MinX", DoubleValue (macroUeBox.xMin));
      Config::SetDefault ("ns3::SteadyStateRandomWaypointMobilityModel::MinY", DoubleValue (macroUeBox.yMin));
      Config::SetDefault ("ns3::SteadyStateRandomWaypointMobilityModel::MaxX", DoubleValue (macroUeBox.xMax));
      Config::SetDefault ("ns3::SteadyStateRandomWaypointMobilityModel::MaxY", DoubleValue (macroUeBox.yMax));
      Config::SetDefault ("ns3::SteadyStateRandomWaypointMobilityModel::Z", DoubleValue (ueZ));
      Config::SetDefault ("ns3::SteadyStateRandomWaypointMobilityModel::MaxSpeed", DoubleValue (outdoorUeMaxSpeed));
      Config::SetDefault ("ns3::SteadyStateRandomWaypointMobilityModel::MinSpeed", DoubleValue (outdoorUeMinSpeed));

      // this is not used since SteadyStateRandomWaypointMobilityModel
      // takes care of initializing the positions;  however we need to
      // reset it since the previously used PositionAllocator
      // (SameRoom) will cause an error when used with homeDeploymentRatio=0
      positionAlloc = CreateObject<RandomBoxPositionAllocator> ();
      mobility.SetPositionAllocator (positionAlloc);
      mobility.Install (macroUes);
      
      // forcing initialization so we don't have to wait for Nodes to
      // start before positions are assigned (which is needed to
      // output node positions to file and to make AttachToClosestEnb work)
      for (NodeContainer::Iterator it = macroUes.Begin ();
           it != macroUes.End ();
           ++it)
        {
          (*it)->Initialize ();
        }
    }
    else
    {
      positionAlloc = CreateObject<RandomBoxPositionAllocator> ();
      Ptr<UniformRandomVariable> xVal = CreateObject<UniformRandomVariable> ();
      xVal->SetAttribute ("Min", DoubleValue (macroUeBox.xMin));
      xVal->SetAttribute ("Max", DoubleValue (macroUeBox.xMax));
      positionAlloc->SetAttribute ("X", PointerValue (xVal));
      Ptr<UniformRandomVariable> yVal = CreateObject<UniformRandomVariable> ();
      yVal->SetAttribute ("Min", DoubleValue (macroUeBox.yMin));
      yVal->SetAttribute ("Max", DoubleValue (macroUeBox.yMax));
      positionAlloc->SetAttribute ("Y", PointerValue (yVal));
      Ptr<UniformRandomVariable> zVal = CreateObject<UniformRandomVariable> ();
      zVal->SetAttribute ("Min", DoubleValue (macroUeBox.zMin));
      zVal->SetAttribute ("Max", DoubleValue (macroUeBox.zMax));
      positionAlloc->SetAttribute ("Z", PointerValue (zVal));
      mobility.SetPositionAllocator (positionAlloc);
      mobility.Install (macroUes);
    }
  BuildingsHelper::Install (macroUes);

  NetDeviceContainer macroUeDevs = lteHelper->InstallUeDevice (macroUes);

  Ipv4Address remoteHostAddr;
  NodeContainer ues;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ipv4InterfaceContainer ueIpIfaces;
  Ptr<Node> remoteHost;
  NetDeviceContainer ueDevs;

  if (epc)
    {
      NS_LOG_LOGIC ("setting up internet and remote host");

      // Create a single RemoteHost
      NodeContainer remoteHostContainer;
      remoteHostContainer.Create (1);
      remoteHost = remoteHostContainer.Get (0);
      InternetStackHelper internet;
      internet.Install (remoteHostContainer);

      // Create the Internet
      PointToPointHelper p2ph;
      p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
      p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
      p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
      Ptr<Node> pgw = epcHelper->GetPgwNode ();
      NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
      Ipv4AddressHelper ipv4h;
      ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
      Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
      // in this container, interface 0 is the pgw, 1 is the remoteHost
      remoteHostAddr = internetIpIfaces.GetAddress (1);

      Ipv4StaticRoutingHelper ipv4RoutingHelper;
      Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
      remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

      // for internetworking purposes, consider together home UEs and macro UEs
      ues.Add (homeUes);
      ues.Add (macroUes);
      ueDevs.Add (homeUeDevs);
      ueDevs.Add (macroUeDevs);

      // Install the IP stack on the UEs
      internet.Install (ues);
      ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

      // attachment (needs to be done after IP stack configuration)
      // using initial cell selection
      lteHelper->Attach (macroUeDevs);
      lteHelper->Attach (homeUeDevs);
    }
  else
    {
      // macro UEs attached to the closest macro eNB
      lteHelper->AttachToClosestEnb (macroUeDevs, macroEnbDevs);

      // each home UE is attached explicitly to its home eNB
      NetDeviceContainer::Iterator ueDevIt;
      NetDeviceContainer::Iterator enbDevIt;
      for (ueDevIt = homeUeDevs.Begin (), enbDevIt = homeEnbDevs.Begin ();
           ueDevIt != homeUeDevs.End (); ++ueDevIt, ++enbDevIt)
        {
          // this because of the order in which SameRoomPositionAllocator
          // will place the UEs
          if (enbDevIt == homeEnbDevs.End ())
            {
              enbDevIt = homeEnbDevs.Begin ();
            }
          lteHelper->Attach (*ueDevIt, *enbDevIt);
        }
    }

  if (epc)
    {
      NS_LOG_LOGIC ("setting up applications");

      // Install and start applications on UEs and remote host
      uint16_t dlPort = 10000;
      uint16_t ulPort = 20000;

      // randomize a bit start times to avoid simulation artifacts
      // (e.g., buffer overflows due to packet transmissions happening
      // exactly at the same time) 
      Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable> ();
      if (useUdp)
        {
          startTimeSeconds->SetAttribute ("Min", DoubleValue (0));
          startTimeSeconds->SetAttribute ("Max", DoubleValue (0.010));
        }
      else
        {
          // TCP needs to be started late enough so that all UEs are connected
          // otherwise TCP SYN packets will get lost
          startTimeSeconds->SetAttribute ("Min", DoubleValue (0.100));
          startTimeSeconds->SetAttribute ("Max", DoubleValue (0.110));
        }

      for (uint32_t u = 0; u < ues.GetN (); ++u)
        {
          Ptr<Node> ue = ues.Get (u);
          // Set the default gateway for the UE
          Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> ());
          ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

          for (uint32_t b = 0; b < numBearersPerUe; ++b)
            {
              ++dlPort;
              ++ulPort;

              ApplicationContainer clientApps;
              ApplicationContainer serverApps;

              if (useUdp)
                {
                  if (epcDl)
                    {
                      NS_LOG_LOGIC ("installing UDP DL app for UE " << u);
                      UdpClientHelper dlClientHelper (ueIpIfaces.GetAddress (u), dlPort);
                      clientApps.Add (dlClientHelper.Install (remoteHost));
                      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), dlPort));
                      serverApps.Add (dlPacketSinkHelper.Install (ue));
                    }
                  if (epcUl)
                    {
                      NS_LOG_LOGIC ("installing UDP UL app for UE " << u);
                      UdpClientHelper ulClientHelper (remoteHostAddr, ulPort);
                      clientApps.Add (ulClientHelper.Install (ue));
                      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), ulPort));
                      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));
                    }
                }
              else // use TCP
                {
                  if (epcDl)
                    {
                      NS_LOG_LOGIC ("installing TCP DL app for UE " << u);
                      BulkSendHelper dlClientHelper ("ns3::TcpSocketFactory",
                                                     InetSocketAddress (ueIpIfaces.GetAddress (u), dlPort));
                      dlClientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
                      clientApps.Add (dlClientHelper.Install (remoteHost));
                      PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), dlPort));
                      serverApps.Add (dlPacketSinkHelper.Install (ue));
                    }
                  if (epcUl)
                    {
                      NS_LOG_LOGIC ("installing TCP UL app for UE " << u);
                      BulkSendHelper ulClientHelper ("ns3::TcpSocketFactory",
                                                     InetSocketAddress (remoteHostAddr, ulPort));
                      ulClientHelper.SetAttribute ("MaxBytes", UintegerValue (0));
                      clientApps.Add (ulClientHelper.Install (ue));
                      PacketSinkHelper ulPacketSinkHelper ("ns3::TcpSocketFactory", 
                                                           InetSocketAddress (Ipv4Address::GetAny (), ulPort));
                      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));
                    }
                } // end if (useUdp)

              Ptr<EpcTft> tft = Create<EpcTft> ();
              if (epcDl)
                {
                  EpcTft::PacketFilter dlpf;
                  dlpf.localPortStart = dlPort;
                  dlpf.localPortEnd = dlPort;
                  tft->Add (dlpf); 
                }
              if (epcUl)
                {
                  EpcTft::PacketFilter ulpf;
                  ulpf.remotePortStart = ulPort;
                  ulpf.remotePortEnd = ulPort;
                  tft->Add (ulpf);
                }

              if (epcDl || epcUl)
                {
                  EpsBearer bearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT);
                  lteHelper->ActivateDedicatedEpsBearer (ueDevs.Get (u), bearer, tft);
                }
              Time startTime = Seconds (startTimeSeconds->GetValue ());
              serverApps.Start (startTime);
              clientApps.Start (startTime);

            } // end for b
        }

    } 
  else // (epc == false)
    {
      // for radio bearer activation purposes, consider together home UEs and macro UEs
      NetDeviceContainer ueDevs;
      ueDevs.Add (homeUeDevs);
      ueDevs.Add (macroUeDevs);
      for (uint32_t u = 0; u < ueDevs.GetN (); ++u)
        {
          Ptr<NetDevice> ueDev = ueDevs.Get (u);
          for (uint32_t b = 0; b < numBearersPerUe; ++b)
            {
              enum EpsBearer::Qci q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
              EpsBearer bearer (q);
              lteHelper->ActivateDataRadioBearer (ueDev, bearer);
            }
        }
    }

  BuildingsHelper::MakeMobilityModelConsistent ();

  Ptr<RadioEnvironmentMapHelper> remHelper;
  if (generateRem)
    {
      PrintGnuplottableBuildingListToFile ("buildings.txt");
      PrintGnuplottableEnbListToFile ("enbs.txt");
      PrintGnuplottableUeListToFile ("ues.txt");

      remHelper = CreateObject<RadioEnvironmentMapHelper> ();
      remHelper->SetAttribute ("ChannelPath", StringValue ("/ChannelList/0"));
      remHelper->SetAttribute ("OutputFile", StringValue ("expr3.rem"));
      remHelper->SetAttribute ("XMin", DoubleValue (macroUeBox.xMin));
      remHelper->SetAttribute ("XMax", DoubleValue (macroUeBox.xMax));
      remHelper->SetAttribute ("YMin", DoubleValue (macroUeBox.yMin));
      remHelper->SetAttribute ("YMax", DoubleValue (macroUeBox.yMax));
      remHelper->SetAttribute ("Z", DoubleValue (1.5));    //用户设备高度(1.5)

      if (remRbId >= 0)
        {
          remHelper->SetAttribute ("UseDataChannel", BooleanValue (true));
          remHelper->SetAttribute ("RbId", IntegerValue (remRbId));
        }

      remHelper->Install ();
      // simulation will stop right after the REM has been generated
    }
  else
    {
      Simulator::Stop (Seconds (simTime));
    }

  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();
  if (epc)
    {
      lteHelper->EnablePdcpTraces ();
    }

  Simulator::Run ();

  //GtkConfigStore config;
  //config.ConfigureAttributes ();

  lteHelper = 0;
  Simulator::Destroy ();
  return 0;
}

