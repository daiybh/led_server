#include "ledServer.h"

#include "LedDll.h"

#define TRACE printf

#include <string>
#include <set>
#include <thread>
#include <mutex>
//#include <format>


#include "stringHelper.h"
#include "logLib.h"
std::mutex queue_mutex;
std::set<std::wstring> clientNetWorkID;

int LedServerCallback(int Msg, int wParam, void* lParam)
{
	SPDLOG_DEBUG("LedServerCallback msg:%d \n", Msg);
	switch (Msg)
	{
	case LV_MSG_CARD_ONLINE:
	{
		LP_CARD_INFO pCardInfo = (LP_CARD_INFO)lParam;
		SPDLOG_DEBUG(L"LV_MSG_CARD_ONLINE port:{} ip:{} networkId:{}\n", pCardInfo->port, pCardInfo->ipStr, pCardInfo->networkIdStr);

		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			clientNetWorkID.emplace(pCardInfo->networkIdStr);
		}
	}
	break;
	case LV_MSG_CARD_OFFLINE:
	{
		LP_CARD_INFO pCardInfo = (LP_CARD_INFO)lParam;

		SPDLOG_DEBUG(L"LV_MSG_CARD_OFFLINE port:{} ip:{} networkId:{}\n", pCardInfo->port, pCardInfo->ipStr, pCardInfo->networkIdStr);

		{
			std::lock_guard<std::mutex> lock(queue_mutex);
			clientNetWorkID.erase(pCardInfo->networkIdStr);
		}
		//((CDemoDlg *)AfxGetApp()->m_pMainWnd)->ComboboxAddString(FALSE,pCardInfo->networkIdStr);
	}
	break;
	}
	return 0;
}

LED_Server::LED_Server()
{

}

int LED_Server::start(int port)
{
	g_Dll = new CLedDll();
	if (!g_Dll->InitDll())
		return -1;

	g_Dll->LV_RegisterLedServerCallback((SERVERINFOCALLBACK)LedServerCallback);
	int ret = g_Dll->LV_LedInitServer(port);

	printf("LV_LedInitServer ret=%d\n", ret);
	return ret;
}

std::string LED_Server::getNetWorkIDList()
{
	std::string htmlContent = "\"idlist\":[";
	{
		std::lock_guard<std::mutex> lock(queue_mutex);

		int i = 0;
		for (auto id : clientNetWorkID)
		{
			if (i != 0)
				htmlContent += ",";
			i++;
			htmlContent += "\"" + to_byte_string(id) + "\"";
		}
	}
	htmlContent += "]";
	return htmlContent;
}

std::tuple<int, std::string> LED_Server::createAProgram(std::wstring& showText)
{
	std::lock_guard<std::mutex> lock(queue_mutex);

	if (clientNetWorkID.empty())
		return std::make_tuple(-1, "networkID is empty\n");

	std::string retHtml = "";

	HPROGRAM hProgram = nullptr;
	for (auto WnetworkID : clientNetWorkID)
	{
		//auto ret = createAProgram(WnetworkID, showText,ledParam);
		if (hProgram == nullptr)
		{
			hProgram = createAProgram_withLspj(showText);
		}
		if (hProgram == nullptr)
		{
			retHtml = "createAProgram_withLspj failed.";
			break;
		}
		auto ret = sendProgram(WnetworkID, hProgram);
		retHtml += std::get<1>(ret);
	}

	return std::make_tuple(0, retHtml);
}

std::tuple<int, std::string> LED_Server::createAProgram2(std::wstring& showText)
{
	std::lock_guard<std::mutex> lock(queue_mutex);

	if (clientNetWorkID.empty())
		return std::make_tuple(-1, "networkID is empty\n");

	std::string retHtml = "";

	HPROGRAM hProgram = nullptr;
	for (auto WnetworkID : clientNetWorkID)
	{
		auto ret = createAProgram(WnetworkID, showText,IConfig.ledParam);
		
		retHtml += std::get<1>(ret);
	}

	return std::make_tuple(0, retHtml);
}

std::tuple<int, std::string> LED_Server::sendProgram(std::wstring WnetworkID, HPROGRAM hProgram)
{
	std::string networkID = to_byte_string(WnetworkID);

	int nResult = 0;
	COMMUNICATIONINFO CommunicationInfo; // 定义一通讯参数结构体变量用于对设定的LED通讯，具体对此结构体元素赋值说明见COMMUNICATIONINFO结构体定义部份注示
	ZeroMemory(&CommunicationInfo, sizeof(COMMUNICATIONINFO));
	CommunicationInfo.LEDType = IConfig.ledParam.ledType;

	CommunicationInfo.SendType = 4;
	_tcscpy(CommunicationInfo.NetworkIdStr, (TCHAR*)WnetworkID.data()); // 指定唯一网络ID


	nResult = g_Dll->LV_Send(&CommunicationInfo, hProgram); // 发送，见函数声明注示
	g_Dll->LV_DeleteProgram(hProgram);					   // 删除节目内存对象，详见函数声明注示
	if (nResult)
	{
		TCHAR ErrStr[256];
		g_Dll->LV_GetError(nResult, 256, ErrStr); // 见函数声明注示
		return std::make_tuple(nResult, fmt::format("{}>>LV_Send ErrStr :{}", networkID, to_byte_string(ErrStr)));
	}
	return std::make_tuple(0, fmt::format("{} setContent sucess", networkID));
}
std::tuple<int, std::string> LED_Server::createAProgram(std::wstring WnetworkID, std::wstring& showText, const Config::LEDParam& ledParam)
{
	std::string networkID = to_byte_string(WnetworkID);

	int nResult = 0;
	COMMUNICATIONINFO CommunicationInfo; // 定义一通讯参数结构体变量用于对设定的LED通讯，具体对此结构体元素赋值说明见COMMUNICATIONINFO结构体定义部份注示
	ZeroMemory(&CommunicationInfo, sizeof(COMMUNICATIONINFO));
	CommunicationInfo.LEDType = ledParam.ledType;

	CommunicationInfo.SendType = 4;
	_tcscpy(CommunicationInfo.NetworkIdStr, (TCHAR*)WnetworkID.data()); // 指定唯一网络ID

	HPROGRAM hProgram;																			 // 节目句柄
	hProgram = g_Dll->LV_CreateProgramEx(ledParam.ledWidth, ledParam.ledHeight, ledParam.ledColor, ledParam.ledGraylevel, 0); // 注意此处屏宽高及颜色参数必需与设置屏参的屏宽高及颜色一致，否则发送时会提示错误
	// 此处可自行判断有未创建成功，hProgram返回NULL失败，非NULL成功,一般不会失败

	nResult = g_Dll->LV_AddProgram(hProgram, 0, 0, 1); // 添加一个节目，参数说明见函数声明注示
	if (nResult)
	{
		TCHAR ErrStr[256];
		g_Dll->LV_GetError(nResult, 256, ErrStr); // 见函数声明注示

		return std::make_tuple(nResult, fmt::format("{}>>LV_AddProgram ErrStr :%s\n", networkID, to_byte_string(ErrStr)));
	}
	AREARECT AreaRect; // 区域坐标属性结构体变量
	AreaRect.left = 0;
	AreaRect.top = 0;
	AreaRect.width = ledParam.ledWidth;
	AreaRect.height = ledParam.ledHeight;

	FONTPROP FontProp; // 文字属性
	ZeroMemory(&FontProp, sizeof(FONTPROP));
	_tcscpy(FontProp.FontName, L"宋体");
	FontProp.FontSize = 12;
	FontProp.FontColor = COLOR_RED;

	nResult = g_Dll->LV_QuickAddSingleLineTextArea(hProgram, 0, 1, &AreaRect, ADDTYPE_STRING, showText.data(), &FontProp, 32); // 快速通过字符添加一个单行文本区域，函数见函数声明注示
	// nResult=g_Dll.LV_QuickAddSingleLineTextArea(hProgram,1,1,&AreaRect,ADDTYPE_FILE,_T("test.rtf"),NULL,4);//快速通过rtf文件添加一个单行文本区域，函数见函数声明注示
	// nResult=g_Dll.LV_QuickAddSingleLineTextArea(hProgram,1,1,&AreaRect,ADDTYPE_FILE,_T("test.txt"),&FontProp,4);//快速通过txt文件添加一个单行文本区域，函数见函数声明注示

	if (nResult)
	{
		TCHAR ErrStr[256];
		g_Dll->LV_GetError(nResult, 256, ErrStr); // 见函数声明注示


		return std::make_tuple(nResult, fmt::format("{}>>LV_QuickAddSingleLineTextArea ErrStr :%s\n", networkID, to_byte_string(ErrStr)));
	}

	nResult = g_Dll->LV_Send(&CommunicationInfo, hProgram); // 发送，见函数声明注示
	g_Dll->LV_DeleteProgram(hProgram);					   // 删除节目内存对象，详见函数声明注示
	if (nResult)
	{
		TCHAR ErrStr[256];
		g_Dll->LV_GetError(nResult, 256, ErrStr); // 见函数声明注示


		return std::make_tuple(nResult, fmt::format("{}>>LV_Send ErrStr :%s\n", networkID, to_byte_string(ErrStr)));
	}
	return std::make_tuple(0, fmt::format("{} setContent sucess", networkID));
}

inline std::vector<std::wstring> split_string(const std::wstring& s, TCHAR delim)
{
	if (s.empty())
		return {};

	std::vector<std::wstring> elems;
	std::wstringstream ss;
	ss.str(s);
	std::wstring item;
	while (std::getline(ss, item, delim))
		elems.push_back(item);
	return elems;
}

HPROGRAM LED_Server::createAProgram_withLspj(std::wstring& showText)
{
	int nResult = 0;
	HPROGRAM hProgram = nullptr;																			 // 节目句柄

	auto stringArr = split_string(showText, L',');
	IConfig.foreach_PGM([&](LED_lsprj& led_lsprj)
		{
			for (auto led : led_lsprj.leds)
			{
				for (auto p : led.programs)
				{
					hProgram = g_Dll->LV_CreateProgramEx(led.LedWidth, led.LedHeight, led.LedColor, led.LedGray, 0); // 注意此处屏宽高及颜色参数必需与设置屏参的屏宽高及颜色一致，否则发送时会提示错误

					nResult = g_Dll->LV_AddProgram(hProgram, 0, 0, 1); // 添加一个节目，参数说明见函数声明注示
					if (nResult)
					{
						TCHAR ErrStr[256];
						g_Dll->LV_GetError(nResult, 256, ErrStr); // 见函数声明注示
						if (hProgram)
						{
							g_Dll->LV_DeleteProgram(hProgram);//删除节目内存对象，详见函数声明注示
							hProgram = nullptr;
						}
						return ;
					}
					for (auto area : p.areas)
					{
						AREARECT AreaRect{
						.left = area.AreaRect_Left,
						.top = area.AreaRect_Top,
						.width = area.AreaRect_Right - area.AreaRect_Left,
						.height = area.AreaRect_Bottom - area.AreaRect_Top,
						};


						FONTPROP FontProp; // 文字属性
						ZeroMemory(&FontProp, sizeof(FONTPROP));
						_tcscpy(FontProp.FontName, L"宋体");
						FontProp.FontSize = 12;
						FontProp.FontColor = COLOR_RED;
						auto pShowText = showText.data();
						if (p.areas.size() > 1 && stringArr.size()==p.areas.size())
							pShowText = stringArr[area.AreaNo-1].data();


						nResult = g_Dll->LV_QuickAddSingleLineTextArea(hProgram, 0, area.AreaNo, &AreaRect, ADDTYPE_STRING, pShowText, &FontProp, 32); // 快速通过字符添加一个单行文本区域，函数见函数声明注示
						// nResult=g_Dll.LV_QuickAddSingleLineTextArea(hProgram,1,1,&AreaRect,ADDTYPE_FILE,_T("test.rtf"),NULL,4);//快速通过rtf文件添加一个单行文本区域，函数见函数声明注示
						// nResult=g_Dll.LV_QuickAddSingleLineTextArea(hProgram,1,1,&AreaRect,ADDTYPE_FILE,_T("test.txt"),&FontProp,4);//快速通过txt文件添加一个单行文本区域，函数见函数声明注示

						if (nResult)
						{
							TCHAR ErrStr[256];
							g_Dll->LV_GetError(nResult, 256, ErrStr); // 见函数声明注示
							if (hProgram)
							{
								g_Dll->LV_DeleteProgram(hProgram);//删除节目内存对象，详见函数声明注示
								hProgram = nullptr;
							}
							return ;
						}
					}

					return ;
				}
			}
		});
	// 此处可自行判断有未创建成功，hProgram返回NULL失败，非NULL成功,一般不会失败


	return hProgram;
}
