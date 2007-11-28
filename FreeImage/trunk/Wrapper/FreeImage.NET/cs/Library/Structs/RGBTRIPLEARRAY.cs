// ==========================================================
// FreeImage 3 .NET wrapper
// Original FreeImage 3 functions and .NET compatible derived functions
//
// Design and implementation by
// - Jean-Philippe Goerke (jpgoerke@users.sourceforge.net)
// - Carsten Klein (cklein05@users.sourceforge.net)
//
// Contributors:
// - David Boland (davidboland@vodafone.ie)
//
// Main reference : MSDN Knowlede Base
//
// This file is part of FreeImage 3
//
// COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN "AS IS" BASIS, WITHOUT WARRANTY
// OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, WITHOUT LIMITATION, WARRANTIES
// THAT THE COVERED CODE IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE
// OR NON-INFRINGING. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED
// CODE IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT, YOU (NOT
// THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE COST OF ANY NECESSARY
// SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL
// PART OF THIS LICENSE. NO USE OF ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER
// THIS DISCLAIMER.
//
// Use at your own risk!
// ==========================================================

// ==========================================================
// CVS
// $Revision: 1.1 $
// $Date: 2007-11-28 15:33:22 $
// $Id: RGBTRIPLEARRAY.cs,v 1.1 2007-11-28 15:33:22 cklein05 Exp $
// ==========================================================

using System;
using System.Collections;
using System.Drawing;
using System.Runtime.InteropServices;

namespace FreeImageAPI
{
	/// <summary>
	/// The structure wraps all operations needed to work with an array of RGBTRIPLEs.
	/// Be aware that the data recieved from the structure are copies, and changes
	/// made to them have to be applied by calling a setter function of the structure.
	/// <para>Two arrays can be compared by their data using the equality or inequality
	/// operators.
	/// The equals(RGBTRIPLEARRAY other)-method can be used to check whether two
	/// arrays map the same block of memory.</para>
	/// </summary>
	public struct RGBTRIPLEARRAY : IComparable, IComparable<RGBTRIPLEARRAY>, IEnumerable, IEquatable<RGBTRIPLEARRAY>
	{
		readonly uint baseAddress;
		readonly uint length;

		/// <summary>
		/// Creates an RGBTRIPLEARRAY structure.
		/// </summary>
		/// <param name="baseAddress">Startaddress of the memory to wrap.</param>
		/// <param name="length">Length of the array.</param>
		public RGBTRIPLEARRAY(IntPtr baseAddress, uint length)
		{
			if (baseAddress == IntPtr.Zero) throw new ArgumentNullException();
			this.baseAddress = (uint)baseAddress;
			this.length = length;
		}

		/// <summary>
		/// Creates an RGBTRIPLEARRAY structure.
		/// </summary>
		/// <param name="dib">Handle to a FreeImage bitmap.</param>
		/// <param name="scanline">Number of the scanline to wrap</param>
		public RGBTRIPLEARRAY(FIBITMAP dib, int scanline)
		{
			if (dib.IsNull) throw new ArgumentNullException();
			if (FreeImage.GetImageType(dib) != FREE_IMAGE_TYPE.FIT_BITMAP) throw new ArgumentException("dib");
			if (FreeImage.GetBPP(dib) != 24) throw new ArgumentException("dib");
			baseAddress = (uint)FreeImage.GetScanLine(dib, scanline);
			length = FreeImage.GetWidth(dib);
		}

		public static bool operator ==(RGBTRIPLEARRAY value1, RGBTRIPLEARRAY value2)
		{
			RGBTRIPLE[] array1 = value1.Data;
			RGBTRIPLE[] array2 = value2.Data;
			if (array1.Length != array2.Length)
				return false;
			for (int i = 0; i < array1.Length; i++)
				if (array1[i] != array2[i])
					return false;
			return true;
		}

		public static bool operator !=(RGBTRIPLEARRAY value1, RGBTRIPLEARRAY value2)
		{
			return !(value1 == value2);
		}

		/// <summary>
		/// Gets the number of elements being wrapped.
		/// </summary>
		public uint Length
		{
			get { return length; }
		}

		/// <summary>
		/// Gets or sets the RGBTRIPLE structure representing the color at the given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <returns>RGBTRIPLE structure of the index.</returns>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe RGBTRIPLE this[int index]
		{
			get
			{
				if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
				return ((RGBTRIPLE*)baseAddress)[index];
			}
			set
			{
				if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
				((RGBTRIPLE*)baseAddress)[index] = value;
			}
		}

		/// <summary>
		/// Returns the color as an UInt32 value.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <returns>An UInt32 value representing the color.</returns>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe uint GetUIntColor(int index)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			return ((uint*)((RGBTRIPLE*)baseAddress + index))[0] & 0x00FFFFFF;
		}

		/// <summary>
		/// Sets the color at position 'index' to the value of 'color'.
		/// </summary>
		/// <param name="index">The index of the color to change.</param>
		/// <param name="color">The new value of the color.</param>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe void SetUIntColor(int index, uint color)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			byte* ptrDestination = (byte*)((RGBTRIPLE*)baseAddress + index);
			byte* ptrSource = (byte*)&color;
			*ptrDestination++ = *ptrSource++;
			*ptrDestination++ = *ptrSource++;
			*ptrDestination++ = *ptrSource++;
		}

		/// <summary>
		/// Returns the color as an RGBTRIPLE structure.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <returns>An RGBTRIPLE structure representing the color.</returns>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe RGBTRIPLE GetRGBTRIPLE(int index)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			return ((RGBTRIPLE*)baseAddress)[index];
		}

		/// <summary>
		/// Sets the color at position 'index' to the value of 'color'.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <param name="color">The new value of the color.</param>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe void SetRGBTRIPLE(int index, RGBTRIPLE color)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			((RGBTRIPLE*)baseAddress)[index] = color;
		}

		/// <summary>
		/// Returns the data representing the red part of the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <returns>The value representing the red part of the color.</returns>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe byte GetRed(int index)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			return ((byte*)((RGBTRIPLE*)baseAddress + index))[FreeImage.FI_RGBA_RED];
		}

		/// <summary>
		/// Sets the red part of the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <param name="red">The new red part of the color.</param>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe void SetRed(int index, byte red)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			((byte*)((RGBTRIPLE*)baseAddress + index))[FreeImage.FI_RGBA_RED] = red;
		}

		/// <summary>
		/// Returns the data representing the green part of the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <returns>The value representing the green part of the color.</returns>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe byte GetGreen(int index)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			return ((byte*)((RGBTRIPLE*)baseAddress + index))[FreeImage.FI_RGBA_GREEN];
		}

		/// <summary>
		/// Sets the green part of the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <param name="green">The new green part of the color.</param>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe void SetGreen(int index, byte green)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			((byte*)((RGBTRIPLE*)baseAddress + index))[FreeImage.FI_RGBA_GREEN] = green;
		}

		/// <summary>
		/// Returns the data representing the blue part of the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <returns>The value representing the blue part of the color.</returns>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe byte GetBlue(int index)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			return ((byte*)((RGBTRIPLE*)baseAddress + index))[FreeImage.FI_RGBA_BLUE];
		}

		/// <summary>
		/// Sets the blue part of the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <param name="blue">The new blue part of the color.</param>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe void SetBlue(int index, byte blue)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			((byte*)((RGBTRIPLE*)baseAddress + index))[FreeImage.FI_RGBA_BLUE] = blue;
		}

		/// <summary>
		/// Returns the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <returns>The color at the index.</returns>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public Color GetColor(int index)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			return GetRGBTRIPLE(index).color;
		}

		/// <summary>
		/// Sets the color at a given index.
		/// </summary>
		/// <param name="index">Index of the color.</param>
		/// <param name="color">The new color.</param>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public void SetColor(int index, Color color)
		{
			if (index >= length || index < 0) throw new ArgumentOutOfRangeException();
			SetRGBTRIPLE(index, new RGBTRIPLE(color));
		}

		/// <summary>
		/// Returns an array of RGBTRIPLE.
		/// Changes to the array will NOT be applied to the bitmap directly.
		/// After all changes have been done, the changes will be applied by
		/// calling the setter of 'Data' with the array.
		/// Keep in mind that using 'Data' is only useful if all values
		/// are being read or/and written.
		/// </summary>
		/// <exception cref="ArgumentOutOfRangeException">
		/// Thrown if index is greater or same as Length</exception>
		public unsafe RGBTRIPLE[] Data
		{
			get
			{
				RGBTRIPLE[] result = new RGBTRIPLE[length];
				for (int i = 0; i < length; i++)
					result[i] = ((RGBTRIPLE*)baseAddress)[i];
				return result;
			}
			set
			{
				if (value.Length != length) throw new ArgumentOutOfRangeException();
				for (int i = 0; i < length; i++)
					((RGBTRIPLE*)baseAddress)[i] = value[i];
			}
		}

		/// <summary>
		/// Compares the current instance with another object of the same type.
		/// </summary>
		/// <param name="obj">An object to compare with this instance.</param>
		/// <returns>A 32-bit signed integer that indicates the relative order of the objects being compared.</returns>
		public int CompareTo(object obj)
		{
			if (!(obj is RGBTRIPLEARRAY))
				throw new ArgumentException();
			return CompareTo((RGBTRIPLEARRAY)obj);
		}

		/// <summary>
		/// Compares the current object with another object of the same type.
		/// </summary>
		/// <param name="other">An object to compare with this object.</param>
		/// <returns>A 32-bit signed integer that indicates the relative order of the objects being compared.</returns>
		public int CompareTo(RGBTRIPLEARRAY other)
		{
			return this.baseAddress.CompareTo(other.baseAddress);
		}

		private class Enumerator : IEnumerator
		{
			private readonly RGBTRIPLEARRAY array;
			private int index = -1;

			public Enumerator(RGBTRIPLEARRAY array)
			{
				this.array = array;
			}

			public object Current
			{
				get
				{
					if (index >= 0 && index <= array.length)
						return array.GetRGBTRIPLE(index);
					throw new InvalidOperationException();
				}
			}

			public bool MoveNext()
			{
				index++;
				if (index < (int)array.length)
					return true;
				return false;
			}

			public void Reset()
			{
				index = -1;
			}
		}

		/// <summary>
		/// Returns an enumerator that iterates through a collection.
		/// </summary>
		/// <returns>An IEnumerator object that can be used to iterate through the collection.</returns>
		public IEnumerator GetEnumerator()
		{
			return new Enumerator(this);
		}

		/// <summary>
		/// Indicates whether the current object is equal to another object of the same type.
		/// </summary>
		/// <param name="other">An object to compare with this object.</param>
		/// <returns>True if the current object is equal to the other parameter; otherwise, false.</returns>
		public bool Equals(RGBTRIPLEARRAY other)
		{
			return ((this.baseAddress == other.baseAddress) && (this.length == other.length));
		}
	}
}