/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010-2011 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

/** @file graphics/images/txb.cpp
 *  TXB (another one of BioWare's own texture formats) loading.
 */

#include "common/util.h"
#include "common/error.h"
#include "common/stream.h"

#include "graphics/images/txb.h"

static const byte kEncodingBGRA = 0x04;
static const byte kEncodingDXT1 = 0x0A;
static const byte kEncodingDXT5 = 0x0C;

namespace Graphics {

TXB::TXB(Common::SeekableReadStream *txb) : _txb(txb),
	_dataSize(0), _txiData(0), _txiDataSize(0) {

	assert(_txb);
}

TXB::~TXB() {
	delete[] _txiData;
}

void TXB::load() {
	if (!_txb)
		return;

	try {

		readHeader(*_txb);
		readData(*_txb);

		_txb->seek(_dataSize + 128);

		readTXIData(*_txb);

		if (_txb->err())
			throw Common::Exception(Common::kReadError);

	} catch (Common::Exception &e) {
		e.add("Failed reading TXB file");
		throw e;
	}

	delete _txb;
	_txb = 0;
}

Common::SeekableReadStream *TXB::getTXI() const {
	if (!_txiData || (_txiDataSize == 0))
		return 0;

	return new Common::MemoryReadStream(_txiData, _txiDataSize);
}

void TXB::readHeader(Common::SeekableReadStream &txb) {
	// Number of bytes for the pixel data in one full image
	uint32 dataSize = txb.readUint32LE();

	_dataSize = dataSize;

	txb.skip(4); // Some float

	// Image dimensions
	uint32 width  = txb.readUint16LE();
	uint32 height = txb.readUint16LE();

	// How's the pixel data encoded?
	byte encoding    = txb.readByte();
	// Number of mip maps in the image
	byte mipMapCount = txb.readByte();

	txb.skip(2); // Unknown (Always 0x0101 on 0x0A and 0x0C types, 0x0100 on 0x09?)
	txb.skip(4); // Some float
	txb.skip(108); // Reserved

	uint32 minDataSize, mipMapSize;
	if        (encoding == kEncodingBGRA) {
		// Raw BGRA

		_compressed = false;
		_hasAlpha   = true;
		_format     = kPixelFormatBGRA;
		_formatRaw  = kPixelFormatRGBA8;
		_dataType   = kPixelDataType8;

		minDataSize = 4;
		mipMapSize  = width * height * 4;

	} else if (encoding == kEncodingDXT1) {
		// S3TC DXT1

		_compressed = true;
		_hasAlpha   = false;
		_format     = kPixelFormatBGR;
		_formatRaw  = kPixelFormatDXT1;
		_dataType   = kPixelDataType8;

		minDataSize = 8;
		mipMapSize  = width * height / 2;

	} else if (encoding == kEncodingDXT5) {
		// S3TC DXT5

		_compressed = true;
		_hasAlpha   = true;
		_format     = kPixelFormatBGRA;
		_formatRaw  = kPixelFormatDXT5;
		_dataType   = kPixelDataType8;

		minDataSize = 16;
		mipMapSize  = width * height;

	} else if (encoding == 0x09)
		// TODO: This seems to be some compression with 8bit per pixel. No min
		//       data size; 2*2 and 1*1 mipmaps seem to be just that big.
		//       Image data doesn't seem to be simple grayscale, paletted,
		//       RGB2222 or RGB332 data either.
		throw Common::Exception("Unsupported TXB encoding 0x09");
	 else
		throw Common::Exception("Unknown TXB encoding 0x%02X (%dx%d, %d, %d)",
				encoding, width, height, mipMapCount, dataSize);

	_mipMaps.reserve(mipMapCount);
	for (uint32 i = 0; i < mipMapCount; i++) {
		MipMap *mipMap = new MipMap;

		mipMap->width  = MAX<uint32>(width,  1);
		mipMap->height = MAX<uint32>(height, 1);

		if (((width < 4) || (height < 4)) && (width != height))
			// Invalid mipmap dimensions
			break;

		mipMap->size = MAX<uint32>(mipMapSize, minDataSize);

		mipMap->data = 0;

		if (dataSize < mipMap->size) {
			// Wouldn't fit
			delete mipMap;
			break;
		}

		dataSize -= mipMap->size;

		_mipMaps.push_back(mipMap);

		width      >>= 1;
		height     >>= 1;
		mipMapSize >>= 2;

		if ((width < 1) && (height < 1))
			break;
	}

}

void TXB::readData(Common::SeekableReadStream &txb) {
	for (std::vector<MipMap *>::iterator mipMap = _mipMaps.begin(); mipMap != _mipMaps.end(); ++mipMap) {
		(*mipMap)->data = new byte[(*mipMap)->size];

		if (txb.read((*mipMap)->data, (*mipMap)->size) != (*mipMap)->size)
			throw Common::Exception(Common::kReadError);
	}
}

void TXB::readTXIData(Common::SeekableReadStream &txb) {
	// TXI data for the rest of the TXB
	_txiDataSize = txb.size() - txb.pos();

	if (_txiDataSize == 0)
		return;

	_txiData = new byte[_txiDataSize];

	if (txb.read(_txiData, _txiDataSize) != _txiDataSize)
		throw Common::Exception(Common::kReadError);
}

} // End of namespace Graphics
