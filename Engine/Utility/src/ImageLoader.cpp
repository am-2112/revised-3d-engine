#pragma once

#include "ImageLoader.h"
#include <wrl.h>

#include <fstream>
#include <functional> //for callback function testing

#include <span>

using namespace Microsoft::WRL;
using namespace std;

namespace Parser {
	//mark the locations of the various bpp formats (since they are ordered within the array)
	const int bpp128 = 0;
	const int bpp64 = 2;
	const int bpp32 = 8;
	const int bpp16 = 12;
	const int bpp8 = 14;

	WICPixelFormatGUID SUPPORTED_PIXEL_TYPES_BY_BPP[] = {
		GUID_WICPixelFormat128bppRGBAFloat,

		GUID_WICPixelFormat64bppRGBAHalf,
		GUID_WICPixelFormat64bppRGBA,

		GUID_WICPixelFormat32bppRGBA,
		GUID_WICPixelFormat32bppBGRA,
		GUID_WICPixelFormat32bppBGR,
		GUID_WICPixelFormat32bppRGBA1010102XR,
		GUID_WICPixelFormat32bppRGBA1010102,
		GUID_WICPixelFormat32bppGrayFloat,

		GUID_WICPixelFormat16bppBGRA5551,
		GUID_WICPixelFormat16bppBGR565,
		GUID_WICPixelFormat16bppGrayHalf,
		GUID_WICPixelFormat16bppGray,

		GUID_WICPixelFormat8bppGray,
		GUID_WICPixelFormat8bppAlpha
	};

	const int bpc8 = 11;
	const int bpc16 = 5;
	const int bpc32 = 1;

	WICPixelFormatGUID SUPPORTED_PIXEL_TYPES_BY_BPC[] = {
		GUID_WICPixelFormat128bppRGBAFloat,
		GUID_WICPixelFormat32bppGrayFloat,

		GUID_WICPixelFormat64bppRGBAHalf,
		GUID_WICPixelFormat64bppRGBA,
		GUID_WICPixelFormat16bppGrayHalf,
		GUID_WICPixelFormat16bppGray,
			
		GUID_WICPixelFormat32bppRGBA,
		GUID_WICPixelFormat32bppBGRA,
		GUID_WICPixelFormat32bppRGBA1010102XR,
		GUID_WICPixelFormat32bppRGBA1010102,
		GUID_WICPixelFormat8bppGray,
		GUID_WICPixelFormat8bppAlpha,

		//these will need custom code
		GUID_WICPixelFormat32bppBGR,
		GUID_WICPixelFormat16bppBGRA5551,
		GUID_WICPixelFormat16bppBGR565
	};

	namespace ImageLoader {
		//what formats can be converted to
		WICPixelFormatGUID NarrowPixelFormat(WICPixelFormatGUID& wicFormatGUID) {
			if (wicFormatGUID == GUID_WICPixelFormatBlackWhite) return GUID_WICPixelFormat8bppGray;
			else if (wicFormatGUID == GUID_WICPixelFormat1bppIndexed) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat2bppIndexed) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat4bppIndexed) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat8bppIndexed) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat2bppGray) return GUID_WICPixelFormat8bppGray;
			else if (wicFormatGUID == GUID_WICPixelFormat4bppGray) return GUID_WICPixelFormat8bppGray;
			else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayFixedPoint) return GUID_WICPixelFormat16bppGrayHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFixedPoint) return GUID_WICPixelFormat32bppGrayFloat;
			else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR555) return GUID_WICPixelFormat16bppBGRA5551;
			else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR101010) return GUID_WICPixelFormat32bppRGBA1010102;
			else if (wicFormatGUID == GUID_WICPixelFormat24bppBGR) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat24bppRGB) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat32bppPBGRA) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat32bppPRGBA) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat48bppRGB) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat48bppBGR) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRA) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBA) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppPBGRA) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat48bppBGRFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;
			else if (wicFormatGUID == GUID_WICPixelFormat128bppPRGBAFloat) return GUID_WICPixelFormat128bppRGBAFloat;
			else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFloat) return GUID_WICPixelFormat128bppRGBAFloat;
			else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;
			else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;
			else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBE) return GUID_WICPixelFormat128bppRGBAFloat;
			else if (wicFormatGUID == GUID_WICPixelFormat32bppCMYK) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppCMYK) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat40bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat80bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)
			else if (wicFormatGUID == GUID_WICPixelFormat32bppRGB) return GUID_WICPixelFormat32bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppRGB) return GUID_WICPixelFormat64bppRGBA;
			else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBAHalf) return GUID_WICPixelFormat64bppRGBAHalf;
#endif

			else return GUID_WICPixelFormatDontCare;
		}

		bool CheckSupportedPixelFormats(WICPixelFormatGUID wicFormatGUID) {
			//wicFormatGUID = NarrowPixelFormat(wicFormatGUID); //don't do this

			int length = sizeof(SUPPORTED_PIXEL_TYPES_BY_BPP) / sizeof(SUPPORTED_PIXEL_TYPES_BY_BPP[0]);
			for (int i = 0; i < length; i++) {
				if (wicFormatGUID == SUPPORTED_PIXEL_TYPES_BY_BPP[i]) return true;
			}
			return false;
		}

		int GetBPP(WICPixelFormatGUID& wicFormatGUID) {
			int i = 0;
			int length = sizeof(SUPPORTED_PIXEL_TYPES_BY_BPP) / sizeof(SUPPORTED_PIXEL_TYPES_BY_BPP[0]);
			for (; i < length; i++) {
				if (wicFormatGUID == SUPPORTED_PIXEL_TYPES_BY_BPP[i]) break;
			}

			if (i <= bpp128) {
				return 128;
			}
			else if (i <= bpp64) {
				return 64;
			}
			else if (i <= bpp32) {
				return 32;
			}
			else if (i <= bpp16) {
				return 16;
			}
			else if (i <= bpp8) {
				return 8;
			}
		}

		//no support yet for the custom 5551 or other such formats
		int GetBPC(WICPixelFormatGUID& wicFormatGUID) {
			int i = 0;
			int length = sizeof(SUPPORTED_PIXEL_TYPES_BY_BPC) / sizeof(SUPPORTED_PIXEL_TYPES_BY_BPC[0]);
			for (; i < length; i++) {
				if (wicFormatGUID == SUPPORTED_PIXEL_TYPES_BY_BPC[i]) break;
			}

			if (i <= bpc32) {
				return 32;
			}
			else if (i <= bpc16) {
				return 16;
			}
			else if (i <= bpc8) {
				return 8;
			}
		}
	}

	namespace ImageLoader {
		bool CheckSupportedPixelFormats(WICPixelFormatGUID wicFormatGUID);
		int GetBPP(WICPixelFormatGUID& wicFormatGUID);
		int GetBPC(WICPixelFormatGUID& wicFormatGUID);
		bool FallbackToWIC(wstring& filePath, MAP& data);

		bool LoadImageFromFile(wstring& filePath, MAP& data) {
			//when the image-library project is finished, it will be used instead of the windows-specific WIC (in a move to make this platform agnostic)
			return FallbackToWIC(filePath, data);
		}

		bool FallbackToWIC(wstring& filePath, MAP& data) {
			HRESULT hr;

			// we only need one instance of the imaging factory to create decoders and frames
			ComPtr<IWICImagingFactory> wicFactory;

			// reset decoder, frame and converter since these will be different for each image we load
			ComPtr<IWICBitmapDecoder> wicDecoder;
			ComPtr<IWICBitmapFrameDecode> wicFrame;
			ComPtr<IWICFormatConverter> wicConverter;

			bool imageConverted = false;

			if (wicFactory == NULL)
			{
				// Initialize the COM library
				hr = CoInitialize(NULL);

				// create the WIC factory
				hr = CoCreateInstance(
					CLSID_WICImagingFactory,
					NULL,
					CLSCTX_INPROC_SERVER,
					IID_PPV_ARGS(&wicFactory)
				);
				if (FAILED(hr)) return false;
			}

			// load a decoder for the image
			hr = wicFactory->CreateDecoderFromFilename(
				filePath.data(),
				NULL,                            //vender ID, no specific one preferred
				GENERIC_READ,
				WICDecodeMetadataCacheOnLoad,
				&wicDecoder
			);

			// get image from decoder (this will decode the "frame")
			hr = wicDecoder->GetFrame(0, &wicFrame);
			if (FAILED(hr)) return false;

			// get wic pixel format of image
			WICPixelFormatGUID pixelFormat;
			hr = wicFrame->GetPixelFormat(&pixelFormat);
			if (FAILED(hr)) return false;

			// get size of image
			UINT textureWidth, textureHeight;
			hr = wicFrame->GetSize(&textureWidth, &textureHeight);
			if (FAILED(hr)) return false;

			//unsupported, need to narrow format and convert (if narrowing succeeds)
			if (!CheckSupportedPixelFormats(pixelFormat)) {
				WICPixelFormatGUID conversion = NarrowPixelFormat(pixelFormat);
				if (conversion == GUID_WICPixelFormatDontCare) return false;

				// create the format converter
				hr = wicFactory->CreateFormatConverter(&wicConverter);
				if (FAILED(hr)) return false;

				//check if able to convert from current to supported format
				BOOL canConvert = FALSE;
				hr = wicConverter->CanConvert(pixelFormat, conversion, &canConvert);
				if (FAILED(hr) || !canConvert) return false;

				//convert (wicConverter contains converted image)
				hr = wicConverter->Initialize(wicFrame.Get(), conversion, WICBitmapDitherTypeErrorDiffusion, 0, 0, WICBitmapPaletteTypeCustom);
				if (FAILED(hr)) return false;

				imageConverted = true;
				pixelFormat = conversion;
			}

			//get bits per pixel, and then stride
			ComPtr<IWICComponentInfo> pIComponentInfo = NULL;
			hr = wicFactory->CreateComponentInfo(pixelFormat, &pIComponentInfo);
			if (FAILED(hr)) return false;

			ComPtr<IWICPixelFormatInfo> pIPixelFormatInfo;
			hr = pIComponentInfo->QueryInterface(__uuidof(IWICPixelFormatInfo), reinterpret_cast<void**>(pIPixelFormatInfo.GetAddressOf()));
			if (FAILED(hr)) return false;

			UINT bpp;
			hr = pIPixelFormatInfo->GetBitsPerPixel(&bpp);
			if (FAILED(hr)) return false;

			int stride = (textureWidth * bpp) / 8;
			int imageSize = stride * textureHeight;

			//fill out data structure
			data.format = pixelFormat;
			data.size = SIZE(textureWidth, textureHeight);
			data.tx = vector<BYTE>(imageSize);
			data.stride = stride;

			if (imageConverted)
			{
				//if image format needed to be converted, the wic converter will contain the converted image
				hr = wicConverter->CopyPixels(0, stride, imageSize, data.tx.data());
				if (FAILED(hr)) return false;
			}
			else
			{
				//no need to convert, just copy data from the wic frame
				hr = wicFrame->CopyPixels(0, stride, imageSize, data.tx.data());
				if (FAILED(hr)) return false;
			}

			return true;
		}		
	}

}