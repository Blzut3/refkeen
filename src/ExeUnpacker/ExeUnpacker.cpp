// EXE unpacker, adapted from OpenTESArena.
// Used for decompressing DOS executables compressed with PKLITE.

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "ExeUnpacker.h"

#include "be_cross.h"

// Replacement for Debug::check
static const void Debug_check(bool expression, const char *msg)
{
	if (!expression)
	{
		BE_Cross_LogMessage(BE_LOG_MSG_ERROR, "%s", msg);
		exit(1);
	}
}

/*** A simple binary tree for retrieving a decoded value, given a vector of bits. ***/

typedef struct Node
{
	int left;
	int right;
	//struct Node *left;
	//struct Node *right;
	int value;
} Node;

typedef Node BitTree;

// Returns a decoded value in the tree. Note that rather than getting
// an input vector of bits, this gets a pointer to a bits fetcher which
// is repeatedly called, once per bit.
template<typename T>
static const int BitTree_get(const BitTree *bt, T getNextBit)
{
	const Node *node = bt;

	// Walk the tree.
	while (true)
	{
		const bool bit = getNextBit();
		// Decide which branch to use.
		if (bit)
		{
			// Right.
			Debug_check(node->right != 0, "Bit Tree - No right branch.\n");
			node += node->right;
		}
		else
		{
			// Left.
			Debug_check(node->left != 0, "Bit Tree - No left branch.\n");
			node += node->left;
		}
		// Check if it's a leaf.
		if ((node->left == 0) && (node->right == 0))
		{
			return node->value;
		}
	}
}

#define ST(l,r) { l, r, -1 } // Node which isn't a leaf
#define LF(v) { 0, 0, v } // Leaf

// Bit table from pklite_specification.md, section 4.3.1 "Number of bytes".
// The decoded value for a given vector is (index + 2) before index 11, and
// (index + 1) after index 11.
//
// The array is illustrated as a binary tree, in which
// every non-leaf node is shown with its subtrees as follows
// (although the root node is "reversed" i.e., L and R are swapped):
//
// N
// 	R
// L
static const Node bitTree1[] =
{
	ST(4,1), // "Reversed" node
		ST(1,2),
			LF(2),
		LF(3),
	ST(1,6),
		ST(1,2),
			LF(4),
		ST(1,2),
			LF(5),
		LF(6),
	ST(1,6),
		ST(1,2),
			LF(7),
		ST(1,2),
			LF(8),
		LF(9),
	ST(1,6),
		ST(1,2),
			LF(10),
		ST(1,2),
			LF(11),
		LF(12),
	ST(1,6),
		ST(1,2),
			LF(25),
		ST(1,2),
			LF(13),
		LF(14),
	ST(1,6),
		ST(1,2),
			LF(15),
		ST(1,2),
			LF(16),
		LF(17),
	ST(1,6),
		ST(1,2),
			LF(18),
		ST(1,2),
			LF(19),
		LF(20),
	ST(1,4),
		ST(1,2),
			LF(21),
		LF(22),
	ST(1,2),
		LF(23),
	LF(24),
};

// Bit table from pklite_specification.md, section 4.3.2 "Offset".
// The decoded value for a given vector is simply its index.
//
// The array is illustrated in a similar manner as before.
static const Node bitTree2[] =
{
	ST(2,1), // "Reversed" node
		LF(0),
	ST(1,12),
		ST(1,4),
			ST(1,2),
				LF(1),
			LF(2),
		ST(1,4),
			ST(1,2),
				LF(3),
			LF(4),
		ST(1,2),
			LF(5),
		LF(6),
	ST(1,18),
		ST(1,8),
			ST(1,4),
				ST(1,2),
					LF(7),
				LF(8),
			ST(1,2),
				LF(9),
			LF(10),
		ST(1,4),
			ST(1,2),
				LF(11),
			LF(12),
		ST(1,2),
			LF(13),
		ST(1,2),
			LF(14),
		LF(15),
	ST(1,16),
		ST(1,8),
			ST(1,4),
				ST(1,2),
					LF(16),
				LF(17),
			ST(1,2),
				LF(18),
			LF(19),
		ST(1,4),
			ST(1,2),
				LF(20),
			LF(21),
		ST(1,2),
			LF(22),
		LF(23),
	ST(1,8),
		ST(1,4),
			ST(1,2),
				LF(24),
			LF(25),
		ST(1,2),
			LF(26),
		LF(27),
	ST(1,4),
		ST(1,2),
			LF(28),
		LF(29),
	ST(1,2),
		LF(30),
	LF(31),
};

namespace
{
	// Bit table from pklite_specification.md, section 4.3.1 "Number of bytes".
	// The decoded value for a given vector is (index + 2) before index 11, and
	// (index + 1) after index 11.
	const std::vector<std::vector<bool>> Duplication1 =
	{
		{ true, false }, // 2
		{ true, true }, // 3
		{ false, false, false }, // 4
		{ false, false, true, false }, // 5
		{ false, false, true, true }, // 6
		{ false, true, false, false }, // 7
		{ false, true, false, true, false }, // 8
		{ false, true, false, true, true }, // 9
		{ false, true, true, false, false }, // 10
		{ false, true, true, false, true, false }, // 11
		{ false, true, true, false, true, true }, // 12
		{ false, true, true, true, false, false }, // Special case
		{ false, true, true, true, false, true, false }, // 13
		{ false, true, true, true, false, true, true }, // 14
		{ false, true, true, true, true, false, false }, // 15
		{ false, true, true, true, true, false, true, false }, // 16
		{ false, true, true, true, true, false, true, true }, // 17
		{ false, true, true, true, true, true, false, false }, // 18
		{ false, true, true, true, true, true, false, true, false }, // 19
		{ false, true, true, true, true, true, false, true, true }, // 20
		{ false, true, true, true, true, true, true, false, false }, // 21
		{ false, true, true, true, true, true, true, false, true }, // 22
		{ false, true, true, true, true, true, true, true, false }, // 23
		{ false, true, true, true, true, true, true, true, true } // 24
	};

	// Bit table from pklite_specification.md, section 4.3.2 "Offset".
	// The decoded value for a given vector is simply its index.
	const std::vector<std::vector<bool>> Duplication2 =
	{
		{ true }, // 0
		{ false, false, false, false }, // 1
		{ false, false, false, true }, // 2
		{ false, false, true, false, false }, // 3
		{ false, false, true, false, true }, // 4
		{ false, false, true, true, false }, // 5
		{ false, false, true, true, true }, // 6
		{ false, true, false, false, false, false }, // 7
		{ false, true, false, false, false, true }, // 8
		{ false, true, false, false, true, false }, // 9
		{ false, true, false, false, true, true }, // 10
		{ false, true, false, true, false, false }, // 11
		{ false, true, false, true, false, true }, // 12
		{ false, true, false, true, true, false }, // 13
		{ false, true, false, true, true, true, false }, // 14
		{ false, true, false, true, true, true, true }, // 15
		{ false, true, true, false, false, false, false }, // 16
		{ false, true, true, false, false, false, true }, // 17
		{ false, true, true, false, false, true, false }, // 18
		{ false, true, true, false, false, true, true }, // 19
		{ false, true, true, false, true, false, false }, // 20
		{ false, true, true, false, true, false, true }, // 21
		{ false, true, true, false, true, true, false }, // 22
		{ false, true, true, false, true, true, true }, // 23
		{ false, true, true, true, false, false, false }, // 24
		{ false, true, true, true, false, false, true }, // 25
		{ false, true, true, true, false, true, false }, // 26
		{ false, true, true, true, false, true, true }, // 27
		{ false, true, true, true, true, false, false }, // 28
		{ false, true, true, true, true, false, true }, // 29
		{ false, true, true, true, true, true, false }, // 30
		{ false, true, true, true, true, true, true } // 31
	};
}

bool ExeUnpacker_unpack(FILE *fp, unsigned char *decompBuff, int buffsize)
{
	BE_Cross_LogMessage(BE_LOG_MSG_NORMAL, "Exe Unpacker (pklite) - unpacking...\n");

	int32_t fileSize = BE_Cross_FileLengthFromHandle(fp);

	std::vector<uint8_t> srcData(fileSize);
	fread(reinterpret_cast<char*>(srcData.data()), srcData.size(), 1, fp);

	// Beginning and end of compressed data in the executable.
	const uint8_t *compressedStart = srcData.data() + 800/*752*/;
	const uint8_t *compressedEnd = srcData.data() + (srcData.size() - 8);

	// Buffer for the decompressed data (also little endian).
	memset(decompBuff, 0, buffsize);

	// Current position for inserting decompressed data.
	unsigned char *decompPtr = decompBuff;

	// A 16-bit array of compressed data.
	uint16_t bitArray = BE_Cross_Swap16LE(*(uint16_t *)compressedStart);

	// Offset from start of compressed data (start at 2 because of the bit array).
	int byteIndex = 2;

	// Number of bits consumed in the current 16-bit array.
	int bitsRead = 0;

	// Continually read bit arrays from the compressed data and interpret each bit. 
	// Break once a compressed byte equals 0xFF in duplication mode.
	while (true)
	{
		// Lambda for getting the next byte from compressed data.
		auto getNextByte = [compressedStart, &byteIndex]()
		{
			const uint8_t byte = compressedStart[byteIndex];
			byteIndex++;

			return byte;
		};

		// Lambda for getting the next bit in the theoretical bit stream.
		auto getNextBit = [&bitArray, &bitsRead, &getNextByte]()
		{
			const bool bit = (bitArray & (1 << bitsRead)) != 0;
			bitsRead++;

			// Advance the bit array if done with the current one.
			if (bitsRead == 16)
			{
				bitsRead = 0;

				// Get two bytes in little endian format.
				const uint8_t byte1 = getNextByte();
				const uint8_t byte2 = getNextByte();
				bitArray = byte1 | (byte2 << 8);
			}

			return bit;
		};

		// Decide which mode to use for the current bit.
		if (getNextBit())
		{
			// "Duplication" mode.
			// Calculate which bytes in the decompressed data to duplicate and append.
#if 1
			int copy = BitTree_get(bitTree1, getNextBit);
#else
			std::vector<bool> copyBits;
			const int *copyPtr = nullptr;

			// Read bits until they match a bit tree leaf.
			while (copyPtr == nullptr)
			{
				copyBits.push_back(getNextBit());
				copyPtr = bitTree1.get(copyBits);
			}
#endif

			// Calculate the number of bytes in the decompressed data to copy.
			uint16_t copyCount = 0;

			// Check for the special bit vector case "011100".
			if (copy == 25) // Special value
			//if (copyBits == Duplication1.at(11))
			{
				// Read a compressed byte.
				const uint8_t encryptedByte = getNextByte();

				if (encryptedByte == 0xFE)
				{
					// Skip the current bit.
					continue;
				}
				else if (encryptedByte == 0xFF)
				{
					// All done with decompression.
					break;
				}
				else
				{
					// Combine the compressed byte with 25 for the byte count.
					copyCount = encryptedByte + 25;
				}
			}
			else
			{
				// Use the decoded value from the first bit table.
#if 1
				copyCount = copy;
#else
				copyCount = *copyPtr;
#endif
			}

			// Calculate the offset in decompressed data. It is a two byte value.
			// The most significant byte is 0 by default.
			uint8_t mostSigByte = 0;

			// If the copy count is not 2, decode the most significant byte.
			if (copyCount != 2)
			{
#if 1
				// Use the decoded value from the second bit table.
				mostSigByte = BitTree_get(bitTree2, getNextBit);
#else
				std::vector<bool> offsetBits;
				const int* offsetPtr = nullptr;

				// Read bits until they match a bit tree leaf.
				while (offsetPtr == nullptr)
				{
					offsetBits.push_back(getNextBit());
					offsetPtr = bitTree2.get(offsetBits);
				}

				// Use the decoded value from the second bit table.
				mostSigByte = *offsetPtr;
#endif
			}

			// Get the least significant byte of the two bytes.
			const uint8_t leastSigByte = getNextByte();

			// Combine the two bytes.
			const uint16_t offset = leastSigByte | (mostSigByte << 8);

			// Finally, duplicate the decompressed data using the calculated offset and size.
			// Note that memcpy or even memmove is NOT the right way,
			// since overlaps are possible
			unsigned char *duplicateBegin = decompPtr - offset;
			unsigned char *duplicateEnd = duplicateBegin + copyCount;
			for (unsigned char *p = duplicateBegin; p < duplicateEnd; ++p, ++decompPtr)
			{
				*decompPtr = *p;
			}
		}
		else
		{
#if 1
			// Get next byte
			const uint8_t decryptedByte = getNextByte();
#else
			// "Decryption" mode.
			// Read the next byte from the compressed data.
			const uint8_t encryptedByte = getNextByte();

			// Lambda for decrypting an encrypted byte with an XOR operation based on 
			// the current bit index. "bitsRead" is between 0 and 15. It is 0 if the
			// 16th bit of the previous array was used to get here.
			auto decrypt = [](uint8_t encryptedByte, int bitsRead)
			{
				//const uint8_t key = 16 - bitsRead;
				const uint8_t decryptedByte = encryptedByte ^ key;
				return decryptedByte;
			};

			// Decrypt the byte.
			const uint8_t decryptedByte = decrypt(encryptedByte, bitsRead);
#endif

			// Append the decrypted byte onto the decompressed data.
			*decompPtr++ = decryptedByte;
		}
	}

	return true;
}