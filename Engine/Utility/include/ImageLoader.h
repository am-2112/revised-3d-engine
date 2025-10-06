#pragma once

#include <wincodec.h>
#include <vector>
#include <string>

namespace Parser {
	struct MAP {
		std::vector<BYTE> tx;
		SIZE size;
		WICPixelFormatGUID format;
		int stride;
	};

	namespace ImageLoader {
		bool LoadImageFromFile(std::wstring& filePath, MAP& data);
		WICPixelFormatGUID NarrowPixelFormat(WICPixelFormatGUID& wicFormatGUID); //after calling this, only need to check SUPPORTED_PIXEL_TYPES when doing things such as converting to DXGI_FORMAT
	}

	WICPixelFormatGUID SUPPORTED_PIXEL_TYPES[];

	enum class SUPPORTED_FILE_FORMATS;
}