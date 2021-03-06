/******************************************************************************
** Copyright (c) 2013-2021 Intel Corporation All Rights Reserved
**
** Licensed under the Apache License, Version 2.0 (the "License"); you may not
** use this file except in compliance with the License.
**
** You may obtain a copy of the License at
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
** WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**
** See the License for the specific language governing permissions and
** limitations under the License.
**
******************************************************************************/

#include "DataManager.h"
#include "EsifServicesInterface.h"
#include "PolicyManagerInterface.h"
#include "DataVaultPath.h"
using namespace TableObjectType;
using namespace std;

DataManager::DataManager(DptfManagerInterface* dptfManager)
	: m_dptfManager(dptfManager)
	, m_tableObjectMap()
	, m_tableRevisions()
{
	loadTableRevisions();
	loadTableObjectMap();
}

DataManager::~DataManager()
{
}

TableObject DataManager::getTableObject(TableObjectType::Type tableType, string uuid)
{
	if (tableObjectExists(tableType))
	{
		auto table = m_tableObjectMap.find(tableType)->second;
		auto dataVaultPaths = table.dataVaultPathForGet();
		for (auto path = dataVaultPaths.begin(); path != dataVaultPaths.end(); path++)
		{
			string nameSpace = path->first;
			string elementPath = path->second;

			if (uuid.empty())
			{
				elementPath = StringParser::replaceAll(elementPath, "/UUID", Constants::EmptyString);
			}
			else
			{
				elementPath = StringParser::replaceAll(elementPath, "UUID", uuid);
			}

			try
			{
				DptfBuffer data = m_dptfManager->getEsifServices()->readConfigurationBinary(nameSpace, elementPath);
				table.setData(data);
				return table;
			}
			catch (...)
			{
			}
		}
	}

	throw dptf_exception("The table object specified was not found.");
}

void DataManager::setTableObject(UInt32 tableDataLength, UInt8* tableData, TableObjectType::Type tableType, string uuid)
{
	if (!tableObjectExists(tableType))
	{
		throw dptf_exception("The table object specified was not found.");
	}

	auto table = m_dptfManager->getDataManager()->getTableObjectMap().find(tableType)->second;
	auto dataVaultPaths = table.dataVaultPathForSet();
	auto path = dataVaultPaths.begin();
	string nameSpace = path->first;
	string elementPath = path->second;

	if (uuid.empty())
	{
		elementPath = StringParser::replaceAll(elementPath, "/UUID", Constants::EmptyString);
	}
	else
	{
		elementPath = StringParser::replaceAll(elementPath, "UUID", uuid);
	}

	try
	{
		m_dptfManager->getEsifServices()->writeConfigurationBinary(
			tableData, tableDataLength, tableDataLength, nameSpace, elementPath);
		sendTableChangedEvent(tableType, uuid);
	}
	catch (...)
	{
	}
}

void DataManager::deleteTableObject(TableObjectType::Type tableType, string uuid)
{
	if (!tableObjectExists(tableType))
	{
		throw dptf_exception("The table object specified was not found.");
	}

	auto table = m_dptfManager->getDataManager()->getTableObjectMap().find(tableType)->second;
	auto dataVaultPaths = table.dataVaultPathForSet();
	auto path = dataVaultPaths.begin();
	string nameSpace = path->first;
	string elementPath = path->second;

	if (uuid.empty())
	{
		elementPath = StringParser::replaceAll(elementPath, "/UUID", Constants::EmptyString);
	}
	else
	{
		elementPath = StringParser::replaceAll(elementPath, "UUID", uuid);
	}

	try
	{
		m_dptfManager->getEsifServices()->deleteConfigurationBinary(nameSpace, elementPath);
		sendTableChangedEvent(tableType, uuid);
	}
	catch (...)
	{
	}
}

void DataManager::deleteAllTableObject(TableObjectType::Type tableType, string uuid)
{
	if (!tableObjectExists(tableType))
	{
		throw dptf_exception("The table object specified was not found.");
	}

	auto table = m_tableObjectMap.find(tableType)->second;
	auto dataVaultPaths = table.dataVaultPathForGet();
	for (auto path = dataVaultPaths.begin(); path != dataVaultPaths.end(); path++)
	{
		string nameSpace = path->first;
		string elementPath = path->second;

		if (uuid.empty())
		{
			elementPath = StringParser::replaceAll(elementPath, "/UUID", Constants::EmptyString);
		}
		else
		{
			elementPath = StringParser::replaceAll(elementPath, "UUID", uuid);
		}

		try
		{
			m_dptfManager->getEsifServices()->deleteConfigurationBinary(nameSpace, elementPath);
			sendTableChangedEvent(tableType, uuid);
		}
		catch (...)
		{
		}
	}
}

Bool DataManager::tableObjectExists(TableObjectType::Type tableType)
{
	auto table = m_tableObjectMap.find(tableType);
	if (table != m_tableObjectMap.end())
	{
		return true;
	}
	return false;
}

UInt32 DataManager::getLatestSupportedTableRevision(TableObjectType::Type tableType)
{
	auto table = m_tableRevisions.find(tableType);
	if (table != m_tableRevisions.end())
	{
		return table->second;
	}

	throw dptf_exception("The table object specified was not found.");
}

map<TableObjectType::Type, TableObject> DataManager::getTableObjectMap()
{
	return m_tableObjectMap;
}

void DataManager::sendTableChangedEvent(TableObjectType::Type tableObjectType, string uuid)
{
	auto policyManager = m_dptfManager->getPolicyManager();
	auto policyIndexes = policyManager->getPolicyIndexes();
	for (auto i = policyIndexes.begin(); i != policyIndexes.end(); ++i)
	{
		try
		{
			auto policy = policyManager->getPolicyPtr(*i);
			if (policy->isDynamicPolicy() == true && policy->getDynamicPolicyUuidString() != uuid)
			{
				continue;
			}

			switch (tableObjectType)
			{
			case TableObjectType::Apat:
				policy->executePolicyAdaptivePerformanceActionsTableChanged();
				break;
			case TableObjectType::Apct:
				policy->executePolicyAdaptivePerformanceConditionsTableChanged();
				break;
			case TableObjectType::Ddrf:
				policy->executePolicyDdrfTableChanged();
				break;
			default:
				break;
			}
		}
		catch (...)
		{
		}
	}
}

void DataManager::loadTableRevisions()
{
	m_tableRevisions.insert({TableObjectType::Apat, 2});
	m_tableRevisions.insert({TableObjectType::Apct, 2});
	m_tableRevisions.insert({TableObjectType::Dynamic_Idsp, 1});
	m_tableRevisions.insert({TableObjectType::Ddrf, 1});
}

void DataManager::loadTableObjectMap()
{
	auto dataVaultString =
		DataVaultPathBasePaths::ExportRoot + "/UUID/" + TableObjectType::ToString(TableObjectType::Apat);
	m_tableObjectMap.insert(
		{TableObjectType::Apat,
		 {TableObjectType::Apat,
		  {{"fld1", "fld1", ESIF_DATA_UINT64},
		   {"fld2", "fld2", ESIF_DATA_STRING},
		   {"fld3", "fld3", ESIF_DATA_STRING},
		   {"fld4", "fld4", ESIF_DATA_UINT64},
		   {"fld5", "fld5", ESIF_DATA_STRING},
		   {"fld6", "fld6", ESIF_DATA_STRING}},
		  {{"override", dataVaultString}, {"dptf", dataVaultString}},
		  {{"override", dataVaultString}}}});

	dataVaultString = DataVaultPathBasePaths::ExportRoot + "/UUID/" + TableObjectType::ToString(TableObjectType::Apct);
	m_tableObjectMap.insert(
		{TableObjectType::Apct,
		 {TableObjectType::Apct,
		  {{"fld1", "fld1", ESIF_DATA_UINT64},	 {"fld2", "fld2", ESIF_DATA_UINT64},
		   {"fld3", "fld3", ESIF_DATA_UINT64},	 {"fld4", "fld4", ESIF_DATA_STRING},
		   {"fld5", "fld5", ESIF_DATA_UINT64},	 {"fld6", "fld6", ESIF_DATA_UINT64},
		   {"fld7", "fld7", ESIF_DATA_UINT64},	 {"fld8", "fld8", ESIF_DATA_UINT64},
		   {"fld9", "fld9", ESIF_DATA_UINT64},	 {"fld10", "fld10", ESIF_DATA_STRING},
		   {"fld11", "fld11", ESIF_DATA_UINT64}, {"fld12", "fld12", ESIF_DATA_UINT64},
		   {"fld13", "fld13", ESIF_DATA_UINT64}, {"fld14", "fld14", ESIF_DATA_UINT64},
		   {"fld15", "fld15", ESIF_DATA_UINT64}, {"fld16", "fld16", ESIF_DATA_STRING},
		   {"fld17", "fld17", ESIF_DATA_UINT64}, {"fld18", "fld18", ESIF_DATA_UINT64},
		   {"fld19", "fld19", ESIF_DATA_UINT64}, {"fld20", "fld20", ESIF_DATA_UINT64},
		   {"fld21", "fld21", ESIF_DATA_UINT64}, {"fld22", "fld22", ESIF_DATA_STRING},
		   {"fld23", "fld23", ESIF_DATA_UINT64}, {"fld24", "fld24", ESIF_DATA_UINT64},
		   {"fld25", "fld25", ESIF_DATA_UINT64}, {"fld26", "fld26", ESIF_DATA_UINT64},
		   {"fld27", "fld27", ESIF_DATA_UINT64}, {"fld28", "fld28", ESIF_DATA_STRING},
		   {"fld29", "fld29", ESIF_DATA_UINT64}, {"fld30", "fld30", ESIF_DATA_UINT64},
		   {"fld31", "fld31", ESIF_DATA_UINT64}, {"fld32", "fld32", ESIF_DATA_UINT64},
		   {"fld33", "fld33", ESIF_DATA_UINT64}, {"fld34", "fld34", ESIF_DATA_STRING},
		   {"fld35", "fld35", ESIF_DATA_UINT64}, {"fld36", "fld36", ESIF_DATA_UINT64},
		   {"fld37", "fld37", ESIF_DATA_UINT64}, {"fld38", "fld38", ESIF_DATA_UINT64},
		   {"fld39", "fld39", ESIF_DATA_UINT64}, {"fld40", "fld40", ESIF_DATA_STRING},
		   {"fld41", "fld41", ESIF_DATA_UINT64}, {"fld42", "fld42", ESIF_DATA_UINT64},
		   {"fld43", "fld43", ESIF_DATA_UINT64}, {"fld44", "fld44", ESIF_DATA_UINT64},
		   {"fld45", "fld45", ESIF_DATA_UINT64}, {"fld46", "fld46", ESIF_DATA_STRING},
		   {"fld47", "fld47", ESIF_DATA_UINT64}, {"fld48", "fld48", ESIF_DATA_UINT64},
		   {"fld49", "fld49", ESIF_DATA_UINT64}, {"fld50", "fld50", ESIF_DATA_UINT64},
		   {"fld51", "fld51", ESIF_DATA_UINT64}, {"fld52", "fld52", ESIF_DATA_STRING},
		   {"fld53", "fld53", ESIF_DATA_UINT64}, {"fld54", "fld54", ESIF_DATA_UINT64},
		   {"fld55", "fld55", ESIF_DATA_UINT64}, {"fld56", "fld56", ESIF_DATA_UINT64},
		   {"fld57", "fld57", ESIF_DATA_UINT64}, {"fld58", "fld58", ESIF_DATA_STRING},
		   {"fld59", "fld59", ESIF_DATA_UINT64}, {"fld60", "fld60", ESIF_DATA_UINT64},
		   {"fld61", "fld61", ESIF_DATA_UINT64}},
		  {{"override", dataVaultString}, {"dptf", dataVaultString}},
		  {{"override", dataVaultString}}}});

	dataVaultString =
		DataVaultPathBasePaths::ExportRoot + "/UUID/" + TableObjectType::ToString(TableObjectType::Dynamic_Idsp);
	m_tableObjectMap.insert(
		{TableObjectType::Dynamic_Idsp,
		 {TableObjectType::Dynamic_Idsp,
		  {{"uuid", "uuid", ESIF_DATA_STRING},
		   {"templateGuid", "templateGuid", ESIF_DATA_STRING},
		   {"policyName", "policyName", ESIF_DATA_STRING}},
		  {{"dptf", dataVaultString}},
		  {{"dptf", dataVaultString}}}});

	dataVaultString = DataVaultPathBasePaths::ExportRoot + "/UUID/" + TableObjectType::ToString(TableObjectType::Ddrf);
	m_tableObjectMap.insert(
		{TableObjectType::Ddrf,
		 {TableObjectType::Ddrf,
		  {{"fld1", "fld1", ESIF_DATA_UINT64},
		   {"fld2", "fld2", ESIF_DATA_UINT64},
		   {"fld3", "fld3", ESIF_DATA_UINT64},
		   {"fld4", "fld4", ESIF_DATA_UINT64},
		   {"fld5", "fld5", ESIF_DATA_UINT64},
		   {"fld6", "fld6", ESIF_DATA_UINT64},
		   {"fld7", "fld7", ESIF_DATA_UINT64},
		   {"fld8", "fld8", ESIF_DATA_UINT64},
		   {"fld9", "fld9", ESIF_DATA_UINT64},
		   {"fld10", "fld10", ESIF_DATA_UINT64},
		   {"fld11", "fld11", ESIF_DATA_UINT64},
		   {"fld12", "fld12", ESIF_DATA_UINT64},
		   {"fld13", "fld13", ESIF_DATA_UINT64},
		   {"fld14", "fld14", ESIF_DATA_UINT64},
		   {"fld15", "fld15", ESIF_DATA_UINT64},
		   {"fld16", "fld16", ESIF_DATA_UINT64},
		   {"fld17", "fld17", ESIF_DATA_UINT64}},
		  {{"override", dataVaultString}, {"dptf", dataVaultString}},
		  {{"override", dataVaultString}}}});
}
