﻿#include "stdafx.h"
#include "UmaEventLibrary.h"

#include <regex>
#include <boost\algorithm\string\trim.hpp>
#include <boost\algorithm\string\replace.hpp>
#include <boost\filesystem.hpp>
#include <boost\optional.hpp>

#include "Utility\CodeConvert.h"
#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"
#include "Utility\Logger.h"


using json = nlohmann::json;
using namespace CodeConvert;


boost::optional<std::wstring> retrieve(
	simstring::reader& dbr,
	const std::vector<std::wstring>& ambiguousEventNames,
	int measure,
	double threshold, double minThreshold
)
{
	// Retrieve similar strings into a string vector.
	std::vector<std::wstring> xstrs;
	for (; threshold > minThreshold/*kMinThreshold*/; threshold -= 0.05) {	// 少なくとも一つが見つかるような閾値を探す
		for (const std::wstring& query : ambiguousEventNames) {
			dbr.retrieve(query, measure, threshold, std::back_inserter(xstrs));
			if (xstrs.size()) {
				break;
			}
		}
		if (xstrs.size()) {
			break;
		}
	}
	if (xstrs.size()) {
		INFO_LOG << L"result: " << xstrs.front() << L" threshold: " << threshold;
		return xstrs.front();
	} else {
		return boost::none;
	}
}

boost::optional<std::wstring> retrieve(
	simstring::reader& dbr,
	const std::vector<std::wstring>& ambiguousEventNames,
	int measure,
	double threshold
)
{
	const double kMinThreshold = 0.4;
	return retrieve(dbr, ambiguousEventNames, measure, threshold, kMinThreshold);
}

void	EventNameNormalize(std::wstring& eventName)
{
	std::wregex rx(L"（進行度[^\\）]+）");
	eventName = std::regex_replace(eventName, rx, L"");
}

// ==============================================================

bool UmaEventLibrary::LoadUmaMusumeLibrary()
{
	m_charaEventList.clear();
	m_supportEventList.clear();
	try {
		auto funcAddjsonEventToUmaEvent = [](const json& jsonEventList, CharaEvent& charaEvent) {
			for (const json& jsonEvent : jsonEventList) {
				auto eventElm = *jsonEvent.items().begin();
				std::wstring eventName = UTF16fromUTF8(eventElm.key());
				EventNameNormalize(eventName);

				charaEvent.umaEventList.emplace_back();
				UmaEvent& umaEvent = charaEvent.umaEventList.back();
				umaEvent.eventName = eventName;

				int i = 0;
				for (const json& jsonOption : eventElm.value()) {
					std::wstring option = UTF16fromUTF8(jsonOption["Option"]);
					std::wstring effect = UTF16fromUTF8(jsonOption["Effect"]);
					boost::algorithm::replace_all(effect, L"\n", L"\r\n");

					if (kMaxOption <= i) {
						ATLASSERT(FALSE);
						throw std::runtime_error("選択肢の数が kMaxOption を超えます");
					}

					umaEvent.eventOptions[i].option = option;
					umaEvent.eventOptions[i].effect = effect;
					++i;
				}
			}
		};

		{	// UmaMusumeLibrary.json
			std::ifstream ifs((GetExeDirectory() / "UmaMusumeLibrary.json").wstring());
			ATLASSERT(ifs);
			if (!ifs) {
				throw std::runtime_error("UmaMusumeLibrary.json の読み込みに失敗");
			}
			json jsonLibrary;
			ifs >> jsonLibrary;

			auto funcLoad = [=](const json& jsonLibrary, const std::string& keyName, CharaEventList& charaEventList) {
				for (const auto& propElm : jsonLibrary[keyName].items()) {
					std::wstring prop = UTF16fromUTF8(propElm.key());	// hosi or rare

					for (const auto& umaElm : propElm.value().items()) {
						std::wstring umaName = UTF16fromUTF8(umaElm.key());

						charaEventList.emplace_back(std::make_unique<CharaEvent>());
						CharaEvent& charaEvent = *charaEventList.back();
						charaEvent.name = umaName;
						charaEvent.property = prop;

						funcAddjsonEventToUmaEvent(umaElm.value()["Event"], charaEvent);
					}
				}
			};
			funcLoad(jsonLibrary, "Charactor", m_charaEventList);
			funcLoad(jsonLibrary, "Support", m_supportEventList);
		}
		{	// UmaMusumeLibraryRevision.json
			std::ifstream ifs((GetExeDirectory() / "UmaMusumeLibraryRevision.json").wstring());
			ATLASSERT(ifs);
			if (!ifs) {
				throw std::runtime_error("UmaMusumeLibraryRevision.json の読み込みに失敗");
			}
			json jsonRevisionLibrary;
			ifs >> jsonRevisionLibrary;
			for (const auto& keyVal : jsonRevisionLibrary.items()) {
				std::wstring sourceName = UTF16fromUTF8(keyVal.key());
				CharaEvent charaEvent;
				charaEvent.name = sourceName;
				funcAddjsonEventToUmaEvent(keyVal.value()["Event"], charaEvent);

				auto funcUpdateEventOptions = [](const CharaEvent& charaEvent, CharaEventList& charaEventList) -> bool {
					for (auto& anotherCharaEventList : charaEventList) {
						if (anotherCharaEventList->name == charaEvent.name) {	// ソース一致
							bool update = false;
							for (auto& anotherUmaEventList : anotherCharaEventList->umaEventList) {
								for (const auto& umaEventList : charaEvent.umaEventList) {
									if (anotherUmaEventList.eventName == umaEventList.eventName) {	// イベント名一致
										anotherUmaEventList.eventOptions = umaEventList.eventOptions;	// 選択肢を上書き
										update = true;
									}
								}
							}
							ATLASSERT(update);
							return true;
						}
					}
					return false;
				};
				// chara/supportEventList へ上書きする
				if (!funcUpdateEventOptions(charaEvent, m_charaEventList)) {
					if (!funcUpdateEventOptions(charaEvent, m_supportEventList)) {
						// イベントリストにイベント名がなかったので、m_supportEventListへ追加しておく
						m_supportEventList.emplace_back(std::make_unique<CharaEvent>(charaEvent));
					}
				}
			}
		}
		{
			std::ifstream ifs((GetExeDirectory() / L"Common.json").wstring());
			ATLASSERT(ifs);
			if (!ifs) {
				throw std::runtime_error("Common.json の読み込みに失敗");
			}
			json jsonCommon;
			ifs >> jsonCommon;
			m_kMinThreshold = jsonCommon["Common"]["Simstring"].value<double>("MinThreshold", m_kMinThreshold);
		}

		_DBUmaNameInit();
		return true;
	}
	catch (std::exception& e) {
		ERROR_LOG << L"LoadUmaMusumeLibrary failed: " << (LPCWSTR)CA2W(e.what());
		ATLASSERT(FALSE);
	}
	return false;
}

void UmaEventLibrary::ChangeIkuseiUmaMusume(const std::wstring& umaName)
{
	std::unique_lock<std::mutex> lock(m_mtxName);	// 一応念のため…
	if (m_currentIkuseUmaMusume != umaName) {
		INFO_LOG << L"ChangeIkuseiUmaMusume: " << umaName;
		m_currentIkuseUmaMusume = umaName;
		m_simstringDBInit = false;
	}
}

void UmaEventLibrary::AnbigiousChangeIkuseImaMusume(std::vector<std::wstring> ambiguousUmaMusumeNames)
{
	// whilte space を取り除く
	for (auto& name : ambiguousUmaMusumeNames) {
		boost::algorithm::trim(name);
		boost::algorithm::replace_all(name, L" ", L"");
		boost::algorithm::replace_all(name, L"\n", L"");
	}

	// Output similar strings from Unicode queries.
	auto optResult = retrieve(*m_dbUmaNameReader, ambiguousUmaMusumeNames, simstring::cosine, 0.6, m_kMinThreshold);
	if (optResult) {
		ChangeIkuseiUmaMusume(optResult.get());
	}
}


boost::optional<UmaEventLibrary::UmaEvent> UmaEventLibrary::AmbiguousSearchEvent(std::vector<std::wstring> ambiguousEventNames)
{
	_DBInit();

	// whilte space を取り除く
	for (auto& name : ambiguousEventNames) {
		boost::algorithm::trim(name);
		boost::algorithm::replace_all(name, L" ", L"");
	}

	// Output similar strings from Unicode queries.
	auto optResult = retrieve(*m_dbReader, ambiguousEventNames, simstring::cosine, 0.6, m_kMinThreshold);
	if (optResult) {
		return _SearchEventOptions(optResult.get());

	} else {
		return boost::none;
	}
}

void UmaEventLibrary::_DBUmaNameInit()
{
	m_dbUmaNameReader = std::make_unique<simstring::reader>();
	auto dbFolder = GetExeDirectory() / L"simstringDB" / L"UmaName";
	auto dbPath = dbFolder / L"umaName_unicode.db";

	// DBフォルダ内を消して初期化
	if (boost::filesystem::is_directory(dbFolder)) {
		boost::system::error_code ec = {};
		boost::filesystem::remove_all(dbFolder, ec);
		if (ec) {
			ERROR_LOG << L"boost::filesystem::remove_all(dbFolder failed: " << (LPCWSTR)CA2W(ec.message().c_str());
		}
	}
	boost::filesystem::create_directories(dbFolder);

	// Open a SimString database for writing (with std::wstring).
	simstring::ngram_generator gen;
	simstring::writer_base<std::wstring> dbw(gen, dbPath.string());

	// 育成ウマ娘の名前を追加
	for (const auto& charaEvent : m_charaEventList) {
		const std::wstring& name = charaEvent->name;
		dbw.insert(name);
	}
	dbw.close();

	// Open the database for reading.
	m_dbUmaNameReader->open(dbPath.string());
}

void UmaEventLibrary::_DBInit()
{
	if (!m_simstringDBInit) {
		m_dbReader = std::make_unique<simstring::reader>();
		auto dbFolder = GetExeDirectory() / L"simstringDB" / L"Event";
		auto dbPath = dbFolder / L"event_unicode.db";

		// DBフォルダ内を消して初期化
		if (boost::filesystem::is_directory(dbFolder)) {
			boost::system::error_code ec = {};
			boost::filesystem::remove_all(dbFolder, ec);
			if (ec) {
				ERROR_LOG << L"boost::filesystem::remove_all(dbFolder failed: " << (LPCWSTR)CA2W(ec.message().c_str());
			}
		}
		boost::filesystem::create_directories(dbFolder);

		// Open a SimString database for writing (with std::wstring).
		simstring::ngram_generator gen(2, false);	// bi-gram
		simstring::writer_base<std::wstring> dbw(gen, dbPath.string());

		// 育成ウマ娘のイベントを追加
		for (const auto& charaEvent : m_charaEventList) {
			if (charaEvent->name.find(m_currentIkuseUmaMusume) == std::wstring::npos) {
				continue;
			}

			for (const auto& umaEvent : charaEvent->umaEventList) {
				dbw.insert(umaEvent.eventName);
			}
			break;
		}

		// サポートカードのイベントを追加
		for (const auto& charaEvent : m_supportEventList) {
			for (const auto& umaEvent : charaEvent->umaEventList) {
				dbw.insert(umaEvent.eventName);
			}
		}
		
		dbw.close();

		// Open the database for reading.
		m_dbReader->open(dbPath.string());

		m_simstringDBInit = true;
	}
}

UmaEventLibrary::UmaEvent UmaEventLibrary::_SearchEventOptions(const std::wstring& eventName)
{
	m_lastEventSource.clear();

	// 育成ウマ娘のイベントを探す
	for (const auto& charaEvent : m_charaEventList) {
		if (charaEvent->name.find(m_currentIkuseUmaMusume) == std::wstring::npos) {
			continue;
		}

		for (const auto& umaEvent : charaEvent->umaEventList) {
			if (umaEvent.eventName == eventName) {
				m_lastEventSource = charaEvent->name;
				return umaEvent;
			}
		}
		break;
	}

	// サポートカードのイベントを探す
	for (const auto& charaEvent : m_supportEventList) {
		for (const auto& umaEvent : charaEvent->umaEventList) {
			if (umaEvent.eventName == eventName) {
				m_lastEventSource = charaEvent->name;
				return umaEvent;
			}
		}
	}
	ATLASSERT(FALSE);
	return UmaEvent();
}
