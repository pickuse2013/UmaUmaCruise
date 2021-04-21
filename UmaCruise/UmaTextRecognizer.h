﻿#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atlmisc.h>
#include "Utility\GdiplusUtil.h"

class UmaTextRecognizer
{
	static constexpr double kEventNameIconThreshold = 0.5;

public:
	bool	LoadSetting();

	std::unique_ptr<Gdiplus::Bitmap>	ScreenShot();

	bool	TextRecognizer(Gdiplus::Bitmap* image = nullptr);

	const std::vector<std::wstring>&	GetUmaMusumeName() const {
		return m_umaMusumeName;
	}
	const std::vector<std::wstring>&	GetCurrentTurn() const {
		return m_currentTurn;
	}
	const std::vector<std::wstring>&	GetEventName() const {
		return m_eventName;
	}
	bool	IsTrainingMenu() const {
		return m_bTrainingMenu;
	}

private:
	// BaseClientSize を基準として、imageのサイズに合うように boundsを調節する
	CRect	_AdjustBounds(const cv::Mat& srcImage, CRect bounds);

	bool	_IsEventNameIcon(cv::Mat srcImage);

	CString	m_targetWindowName;
	CString m_targetClassName;

	CSize	m_baseClientSize;

	enum TestBounds {
		kUmaMusumeSubNameBounds, kUmaMusumeNameBounds, kCurrentTurnBounds, kEventCategoryBounds, kEventNameBounds, kEventNameIconBounds, kCurrentMenuBounds, kBackButtonBounds, kMaxCount
	};
	static constexpr LPCWSTR kTestBoundsName[kMaxCount] = {
		L"UmaMusumeSubNameBounds", L"UmaMusumeNameBounds", L"CurrentTurnBounds", L"EventCategoryBounds", L"EventNameBounds", L"EventNameIconBounds", L"CurrentMenuBounds", L"BackButtonBounds", 
	};
	std::array<CRect, kMaxCount>	m_testBounds;
	int		m_kCurrentTurnThreshold = 111;
	std::unordered_map<std::wstring, std::wstring>	m_typoDictionary;

	std::vector<std::wstring>	m_umaMusumeName;
	std::vector<std::wstring>	m_currentTurn;
	std::vector<std::wstring>	m_eventName;
	bool						m_bTrainingMenu = false;
};

