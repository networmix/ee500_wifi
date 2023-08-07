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

#include <map>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/application.h"
#include "ns3/stats-module.h"

using namespace ns3;

class LocalDataOutput : public ns3::DataOutputInterface
{
public:
    static ns3::TypeId GetTypeId(void);

    virtual void Output(ns3::DataCollector &dc);

    const std::map<std::string, double> &GetCounters() const;
    const std::map<std::string, std::string> &GetMetadata() const;

private:
    class LocalOutputCallback : public ns3::DataOutputCallback
    {
    public:
        LocalOutputCallback(LocalDataOutput *owner);

        // Inherited via DataOutputCallback, need to implement all of them
        virtual void OutputSingleton(std::string key, std::string variable, int val);
        virtual void OutputSingleton(std::string key, std::string variable, double val);
        virtual void OutputSingleton(std::string key, std::string variable, uint32_t val);
        virtual void OutputSingleton(std::string key, std::string variable, std::string val);
        virtual void OutputSingleton(std::string key, std::string variable, ns3::Time val);
        virtual void OutputStatistic(std::string key, std::string variable, const ns3::StatisticalSummary *statSum);

    private:
        LocalDataOutput *m_owner;
    };

    std::map<std::string, double> m_counters;
    std::map<std::string, std::string> m_metadata;
};
