#pragma once

#include <string>
#include <vector>

//can use the same namespace across all the parsers to extend it
//also, by not including any of the other parsers (only in the actual .cpp implementation), it also means that all of their methods are hidden to a program only including the wrapper
//the idea here is to provide a general interface for returning parsed data as well as inputting files to be parsed (ie. automatically detecting the extension and calling the right subroutines)
namespace Parser {
	namespace Wrapper {
		struct Object_3D {
			std::vector<int> objectGroup; //for a specified object group

		};

		//properties that differ between both objects loaded from same file structure, and those from different structures
		enum class OBJECT_PROPERTIES {
			OBJECT_PROPERTIES_
		};

		//supported 3d file types
		enum SUPPORTED_FILES {
			obj
		};

		bool LoadFile(std::wstring filePath, Object_3D &object);
	}
}