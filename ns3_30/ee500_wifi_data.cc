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
 *  Authors: Andrey Golovanov, Jul 2023 for EE500 at DCU <networmix@gmail.com>
 *
 */

#include "ee500_wifi_data.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LocalDataOutput");

LocalDataOutput::LocalOutputCallback::LocalOutputCallback(LocalDataOutput *owner) : m_owner(owner) {}

ns3::TypeId LocalDataOutput::GetTypeId(void)
{
    static ns3::TypeId tid = ns3::TypeId("LocalDataOutput")
                                 .SetParent<DataOutputInterface>()
                                 .SetGroupName("Network")
                                 .AddConstructor<LocalDataOutput>();
    return tid;
}

void LocalDataOutput::LocalOutputCallback::OutputSingleton(std::string key, std::string variable, int val)
{
    m_owner->m_counters[variable + "_" + key] = val;
}

void LocalDataOutput::LocalOutputCallback::OutputSingleton(std::string key, std::string variable, uint32_t val)
{
    // For uint32_t, we can just static_cast to double
    m_owner->m_counters[variable + "_" + key] = static_cast<double>(val);
}

void LocalDataOutput::LocalOutputCallback::OutputSingleton(std::string key, std::string variable, double val)
{
    m_owner->m_counters[variable + "_" + key] = val;
}

void LocalDataOutput::LocalOutputCallback::OutputSingleton(std::string key, std::string variable, std::string val)
{
    // Here we assume that the string val is always convertible to double.
    // If not, we just ignore the value
    try
    {
        double value = std::stod(val);
        m_owner->m_counters[variable + "_" + key] = value;
    }
    catch (const std::exception &e)
    {
        // If the conversion fails, we just ignore the value
        NS_LOG_ERROR("Exception in OutputSingleton (string): " << e.what());
    }
}

void LocalDataOutput::LocalOutputCallback::OutputSingleton(std::string key, std::string variable, ns3::Time val)
{
    // For ns3::Time, we convert to seconds and then to double
    m_owner->m_counters[variable + "_" + key] = val.GetSeconds();
}

void LocalDataOutput::LocalOutputCallback::OutputStatistic(std::string key, std::string variable, const ns3::StatisticalSummary *statSum)
{
    std::string baseKey = variable + "_" + key;
    m_owner->m_counters[baseKey + "_min"] = statSum->getMin();
    m_owner->m_counters[baseKey + "_max"] = statSum->getMax();
    m_owner->m_counters[baseKey + "_avg"] = statSum->getMean();
    m_owner->m_counters[baseKey + "_sum"] = statSum->getSum();
}

void LocalDataOutput::Output(ns3::DataCollector &dc)
{
    // Iterate over all calculators in the DataCollector
    for (auto it = dc.DataCalculatorBegin(); it != dc.DataCalculatorEnd(); ++it)
    {
        // Check the type of the calculator
        if (ns3::CounterCalculator<double> *counterCalc = dynamic_cast<ns3::CounterCalculator<double> *>(PeekPointer(*it)))
        {
            std::string key = counterCalc->GetKey() + "_" + counterCalc->GetContext();
            m_counters[key] = counterCalc->GetCount();
        }
        else if (ns3::CounterCalculator<uint32_t> *counterCalcUint32 = dynamic_cast<ns3::CounterCalculator<uint32_t> *>(PeekPointer(*it)))
        {
            std::string key = counterCalcUint32->GetKey() + "_" + counterCalcUint32->GetContext();
            m_counters[key] = counterCalcUint32->GetCount();
        }
        else if (ns3::PacketCounterCalculator *packetCounterCalc = dynamic_cast<ns3::PacketCounterCalculator *>(PeekPointer(*it)))
        {
            std::string key = packetCounterCalc->GetKey() + "_" + packetCounterCalc->GetContext();
            m_counters[key] = packetCounterCalc->GetCount();
        }
        else if (ns3::TimeMinMaxAvgTotalCalculator *timeCalc = dynamic_cast<ns3::TimeMinMaxAvgTotalCalculator *>(PeekPointer(*it)))
        {
            // For TimeMinMaxAvgTotalCalculator we use Output method
            LocalOutputCallback callback(this);
            timeCalc->Output(callback);
        }
        // If you have other types of calculators, add them here
    }

    // Iterate over all metadata in the DataCollector
    for (auto it = dc.MetadataBegin(); it != dc.MetadataEnd(); ++it)
    {
        // Store the metadata key-value pair in the map
        m_metadata[it->first] = it->second;
    }
}

const std::map<std::string, double> &
LocalDataOutput::GetCounters() const
{
    return m_counters;
}

const std::map<std::string, std::string> &
LocalDataOutput::GetMetadata() const
{
    return m_metadata;
}
