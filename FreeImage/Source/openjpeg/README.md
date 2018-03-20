OpenJpeg library
=================

Installation instructions here: https://github.com/uclouvain/openjpeg/blob/master/INSTALL.md

Here's what I did,

    cd "\Program Files (x86)\Microsoft Visual Studio 14.0\VC"
	vsvarsall amd64
	cd "\openjpeg"
	mkdir build
	cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE:string="Release" -DBUILD_SHARED_LIBS:bool=off ..
	
Then I copied the lib over after building.

I tested this new FreeImage DLL in my project but it doesn't fix the issue I was trying to fix:
https://stackoverflow.com/questions/49363134/not-safari-friendly-alpha-with-freeimage-jp2-export


