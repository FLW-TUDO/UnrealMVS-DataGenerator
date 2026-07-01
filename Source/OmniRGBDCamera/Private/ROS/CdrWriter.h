#pragma once

#include "CoreMinimal.h"
#include <cstring>

/**
 * Minimal CDR (Common Data Representation) serializer for ROS2 messages.
 *
 * Layout:
 *   Byte 0-3  : CDR encapsulation header (0x00 0x01 0x00 0x00 = little-endian)
 *   Byte 4+   : serialized fields with CDR alignment rules
 *
 * Alignment is measured from the body start (Buffer.Num() - 4).
 * This matches the OMG CDR specification used by ROS2 (rcl/rmw).
 */
struct FCdrWriter
{
	TArray<uint8> Buffer;

	FCdrWriter()
	{
		// CDR encapsulation header: little-endian representation (0x00 0x01) + padding (0x00 0x00)
		Buffer.Add(0x00);
		Buffer.Add(0x01);
		Buffer.Add(0x00);
		Buffer.Add(0x00);
	}

	/** Number of bytes written into the message body (excludes the 4-byte header). */
	int32 BodyOffset() const { return Buffer.Num() - 4; }

	/** Inserts zero-padding bytes so BodyOffset() becomes a multiple of Alignment. */
	void Pad(int32 Alignment)
	{
		const int32 Rem = BodyOffset() % Alignment;
		if (Rem != 0)
		{
			const int32 PadBytes = Alignment - Rem;
			for (int32 i = 0; i < PadBytes; i++)
				Buffer.Add(0x00);
		}
	}

	// -------------------------------------------------------------------------
	// Primitive writers

	void WriteUInt8(uint8 V)
	{
		Buffer.Add(V);
	}

	void WriteBool(bool V)
	{
		Buffer.Add(V ? 1 : 0);
	}

	void WriteUInt32(uint32 V)
	{
		Pad(4);
		uint8 Bytes[4];
		FMemory::Memcpy(Bytes, &V, 4);
		Buffer.Append(Bytes, 4);
	}

	void WriteInt32(int32 V)
	{
		Pad(4);
		uint8 Bytes[4];
		FMemory::Memcpy(Bytes, &V, 4);
		Buffer.Append(Bytes, 4);
	}

	void WriteFloat64(double V)
	{
		Pad(8);
		uint8 Bytes[8];
		FMemory::Memcpy(Bytes, &V, 8);
		Buffer.Append(Bytes, 8);
	}

	// -------------------------------------------------------------------------
	// String: uint32 length (including null terminator) + UTF-8 chars + '\0'

	void WriteString(const FString& S)
	{
		const auto Utf8 = StringCast<UTF8CHAR>(*S);
		const uint32 CharCount = (uint32)Utf8.Length();
		WriteUInt32(CharCount + 1);                      // length includes '\0'
		Buffer.Append((const uint8*)Utf8.Get(), CharCount);
		Buffer.Add(0x00);                                // null terminator
	}

	void WriteString(const char* S)
	{
		const uint32 Len = (uint32)strlen(S);
		WriteUInt32(Len + 1);
		Buffer.Append((const uint8*)S, Len);
		Buffer.Add(0x00);
	}

	// -------------------------------------------------------------------------
	// Arrays

	/**
	 * Fixed-length float64 array (e.g. float64[36] covariance).
	 * CDR: align to 8, then raw double data — NO length prefix.
	 */
	void WriteFloat64FixedArray(const double* Values, int32 Count)
	{
		Pad(8);
		for (int32 i = 0; i < Count; i++)
		{
			uint8 Bytes[8];
			FMemory::Memcpy(Bytes, &Values[i], 8);
			Buffer.Append(Bytes, 8);
		}
	}

	/**
	 * Variable-length float64 sequence (e.g. float64[] D).
	 * CDR: uint32 count + align to 8 + raw double data.
	 */
	void WriteFloat64Sequence(const double* Values, int32 Count)
	{
		WriteUInt32((uint32)Count);
		if (Count > 0)
		{
			Pad(8);
			for (int32 i = 0; i < Count; i++)
			{
				uint8 Bytes[8];
				FMemory::Memcpy(Bytes, &Values[i], 8);
				Buffer.Append(Bytes, 8);
			}
		}
	}

	/**
	 * Variable-length uint8 sequence (e.g. uint8[] data in sensor_msgs/Image).
	 * CDR: uint32 count + raw uint8 data (no alignment needed for uint8).
	 */
	void WriteUInt8Sequence(const uint8* Data, int32 Count)
	{
		WriteUInt32((uint32)Count);
		if (Count > 0)
			Buffer.Append(Data, Count);
	}
};
