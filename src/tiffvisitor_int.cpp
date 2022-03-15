// SPDX-License-Identifier: GPL-2.0-or-later

// included header files
#include "config.h"

#include "tiffcomposite_int.hpp" // Do not change the order of these 2 includes,
#include "tiffvisitor_int.hpp"   // see bug #487
#include "tiffimage_int.hpp"
#include "makernote_int.hpp"
#include "exif.hpp"
#include "enforce.hpp"
#include "iptc.hpp"
#include "value.hpp"
#include "jpgimage.hpp"
#include "sonymn_int.hpp"

#include <iostream>

// *****************************************************************************
namespace {
    //! Unary predicate that matches an Exifdatum with a given group and index.
    class FindExifdatum2 {
    public:
        //! Constructor, initializes the object with the group and index to look for.
        FindExifdatum2(Exiv2::Internal::IfdId group, int idx)
            : groupName_(Exiv2::Internal::groupName(group)), idx_(idx) {}
        //! Returns true if group and index match.
        bool operator()(const Exiv2::Exifdatum& md) const
        {
            return idx_ == md.idx() && 0 == strcmp(md.groupName().c_str(), groupName_);
        }

    private:
        const char* groupName_;
        int idx_;

    }; // class FindExifdatum2

    Exiv2::ByteOrder stringToByteOrder(const std::string& val)
    {
        Exiv2::ByteOrder bo = Exiv2::invalidByteOrder;
        if (0 == strcmp("II", val.c_str())) bo = Exiv2::littleEndian;
        else if (0 == strcmp("MM", val.c_str())) bo = Exiv2::bigEndian;

        return bo;
    }
}  // namespace

// *****************************************************************************
// class member definitions
namespace Exiv2::Internal {

    TiffVisitor::TiffVisitor()
    {
        go_.fill(true);
    }

    void TiffVisitor::setGo(GoEvent event, bool go)
    {
        assert(event >= 0 && static_cast<int>(event) < events_);
        go_[event] = go;
    }

    bool TiffVisitor::go(GoEvent event) const
    {
        assert(event >= 0 && static_cast<int>(event) < events_);
        return go_[event];
    }

    void TiffVisitor::visitDirectoryNext(TiffDirectory* /*object*/)
    {
    }

    void TiffVisitor::visitDirectoryEnd(TiffDirectory* /*object*/)
    {
    }

    void TiffVisitor::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/)
    {
    }

    void TiffVisitor::visitBinaryArrayEnd(TiffBinaryArray* /*object*/)
    {
    }

    void TiffFinder::init(uint16_t tag, IfdId group)
    {
        tag_ = tag;
        group_ = group;
        tiffComponent_ = nullptr;
        setGo(geTraverse, true);
    }

    void TiffFinder::findObject(TiffComponent* object)
    {
        if (object->tag() == tag_ && object->group() == group_) {
            tiffComponent_ = object;
            setGo(geTraverse, false);
        }
    }

    void TiffFinder::visitEntry(TiffEntry* object)
    {
        findObject(object);
    }

    void TiffFinder::visitDataEntry(TiffDataEntry* object)
    {
        findObject(object);
    }

    void TiffFinder::visitImageEntry(TiffImageEntry* object)
    {
        findObject(object);
    }

    void TiffFinder::visitSizeEntry(TiffSizeEntry* object)
    {
        findObject(object);
    }

    void TiffFinder::visitDirectory(TiffDirectory* object)
    {
        findObject(object);
    }

    void TiffFinder::visitSubIfd(TiffSubIfd* object)
    {
        findObject(object);
    }

    void TiffFinder::visitMnEntry(TiffMnEntry* object)
    {
        findObject(object);
    }

    void TiffFinder::visitIfdMakernote(TiffIfdMakernote* object)
    {
        findObject(object);
    }

    void TiffFinder::visitBinaryArray(TiffBinaryArray* object)
    {
        findObject(object);
    }

    void TiffFinder::visitBinaryElement(TiffBinaryElement* object)
    {
        findObject(object);
    }

    TiffCopier::TiffCopier(      TiffComponent*  pRoot,
                                 uint32_t        root,
                           const TiffHeaderBase* pHeader,
                           const PrimaryGroups*  pPrimaryGroups)
        : pRoot_(pRoot),
          root_(root),
          pHeader_(pHeader),
          pPrimaryGroups_(pPrimaryGroups)
    {
        assert(pRoot_);
        assert(pHeader_);
        assert(pPrimaryGroups_);
    }

    void TiffCopier::copyObject(TiffComponent* object)
    {
        assert(object);

        if (pHeader_->isImageTag(object->tag(), object->group(), pPrimaryGroups_)) {
            auto clone = object->clone();
            // Assumption is that the corresponding TIFF entry doesn't exist
            TiffPath tiffPath;
            TiffCreator::getPath(tiffPath, object->tag(), object->group(), root_);
            pRoot_->addPath(object->tag(), tiffPath, pRoot_, std::move(clone));
#ifdef EXIV2_DEBUG_MESSAGES
            ExifKey key(object->tag(), groupName(object->group()));
            std::cerr << "Copied " << key << "\n";
#endif
        }
    }

    void TiffCopier::visitEntry(TiffEntry* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitDataEntry(TiffDataEntry* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitImageEntry(TiffImageEntry* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitSizeEntry(TiffSizeEntry* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitDirectory(TiffDirectory* /*object*/)
    {
        // Do not copy directories (avoids problems with SubIfds)
    }

    void TiffCopier::visitSubIfd(TiffSubIfd* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitMnEntry(TiffMnEntry* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitIfdMakernote(TiffIfdMakernote* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitBinaryArray(TiffBinaryArray* object)
    {
        copyObject(object);
    }

    void TiffCopier::visitBinaryElement(TiffBinaryElement* object)
    {
        copyObject(object);
    }

    TiffDecoder::TiffDecoder(
        ExifData&            exifData,
        IptcData&            iptcData,
        XmpData&             xmpData,
        TiffComponent* const pRoot,
        FindDecoderFct       findDecoderFct
    )
        : exifData_(exifData),
          iptcData_(iptcData),
          xmpData_(xmpData),
          pRoot_(pRoot),
          findDecoderFct_(findDecoderFct),
          decodedIptc_(false)
    {
        assert(pRoot);

        // #1402 Fujifilm RAF. Search for the make
        // Find camera make in existing metadata (read from the JPEG)
        ExifKey key("Exif.Image.Make");
        if ( exifData_.findKey(key) != exifData_.end( ) ){
            make_ = exifData_.findKey(key)->toString();
        } else {
            // Find camera make by looking for tag 0x010f in IFD0
            TiffFinder finder(0x010f, ifd0Id);
            pRoot_->accept(finder);
            auto te = dynamic_cast<TiffEntryBase*>(finder.result());
            if (te && te->pValue()) {
                make_ = te->pValue()->toString();
            }
        }
    }

    void TiffDecoder::visitEntry(TiffEntry* object)
    {
        decodeTiffEntry(object);
    }

    void TiffDecoder::visitDataEntry(TiffDataEntry* object)
    {
        decodeTiffEntry(object);
    }

    void TiffDecoder::visitImageEntry(TiffImageEntry* object)
    {
        decodeTiffEntry(object);
    }

    void TiffDecoder::visitSizeEntry(TiffSizeEntry* object)
    {
        decodeTiffEntry(object);
    }

    void TiffDecoder::visitDirectory(TiffDirectory* /* object */ )
    {
        // Nothing to do
    }

    void TiffDecoder::visitSubIfd(TiffSubIfd* object)
    {
        decodeTiffEntry(object);
    }

    void TiffDecoder::visitMnEntry(TiffMnEntry* object)
    {
        // Always decode binary makernote tag
        decodeTiffEntry(object);
    }

    void TiffDecoder::visitIfdMakernote(TiffIfdMakernote* object)
    {
        assert(object);

        exifData_["Exif.MakerNote.Offset"] = object->mnOffset();
        switch (object->byteOrder()) {
        case littleEndian:
            exifData_["Exif.MakerNote.ByteOrder"] = "II";
            break;
        case bigEndian:
            exifData_["Exif.MakerNote.ByteOrder"] = "MM";
            break;
        case invalidByteOrder:
            assert(object->byteOrder() != invalidByteOrder);
            break;
        }
    }

    void TiffDecoder::getObjData(byte const*& pData,
                                 size_t &size,
                                 uint16_t tag,
                                 IfdId group,
                                 const TiffEntryBase* object)
    {
        if (object && object->tag() == tag && object->group() == group) {
            pData = object->pData();
            size = object->size();
            return;
        }
        TiffFinder finder(tag, group);
        pRoot_->accept(finder);
        TiffEntryBase const* te = dynamic_cast<TiffEntryBase*>(finder.result());
        if (te) {
            pData = te->pData();
            size = te->size();
            return;
        }
    }

    void TiffDecoder::decodeXmp(const TiffEntryBase* object)
    {
        // add Exif tag anyway
        decodeStdTiffEntry(object);

        byte const* pData = nullptr;
        size_t size = 0;
        getObjData(pData, size, 0x02bc, ifd0Id, object);
        if (pData) {
            std::string xmpPacket;
            xmpPacket.assign(reinterpret_cast<const char*>(pData), size);
            std::string::size_type idx = xmpPacket.find_first_of('<');
            if (idx != std::string::npos && idx > 0) {
#ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Removing " << static_cast<unsigned long>(idx)
                            << " characters from the beginning of the XMP packet\n";
#endif
                xmpPacket = xmpPacket.substr(idx);
            }
            if (XmpParser::decode(xmpData_, xmpPacket)) {
#ifndef SUPPRESS_WARNINGS
                EXV_WARNING << "Failed to decode XMP metadata.\n";
#endif
            }
        }
    } // TiffDecoder::decodeXmp

    void TiffDecoder::decodeIptc(const TiffEntryBase* object)
    {
        // add Exif tag anyway
        decodeStdTiffEntry(object);

        // All tags are read at this point, so the first time we come here,
        // find the relevant IPTC tag and decode IPTC if found
        if (decodedIptc_) {
            return;
        }
        decodedIptc_ = true;
        // 1st choice: IPTCNAA
        byte const* pData = nullptr;
        size_t size = 0;
        getObjData(pData, size, 0x83bb, ifd0Id, object);
        if (pData) {
            if (0 == IptcParser::decode(iptcData_, pData, static_cast<uint32_t>(size))) {
                return;
            }
#ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Failed to decode IPTC block found in "
                        << "Directory Image, entry 0x83bb\n";

#endif
        }

        // 2nd choice if no IPTCNAA record found or failed to decode it:
        // ImageResources
        pData = nullptr;
        size = 0;
        getObjData(pData, size, 0x8649, ifd0Id, object);
        if (pData) {
            byte const* record = nullptr;
            uint32_t sizeHdr = 0;
            uint32_t sizeData = 0;
            if (0 != Photoshop::locateIptcIrb(pData, size, &record, &sizeHdr, &sizeData)) {
                return;
            }
            if (0 == IptcParser::decode(iptcData_, record + sizeHdr, sizeData)) {
                return;
            }
#ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Failed to decode IPTC block found in "
                        << "Directory Image, entry 0x8649\n";

#endif
        }
    } // TiffMetadataDecoder::decodeIptc

    static const TagInfo* findTag(const TagInfo* pList,uint16_t tag)
    {
        while ( pList->tag_ != 0xffff && pList->tag_ != tag ) pList++;
        return pList->tag_ != 0xffff ? pList : nullptr;
    }

    void TiffDecoder::decodeCanonAFInfo(const TiffEntryBase* object) {
        // report Exif.Canon.AFInfo as usual
        TiffDecoder::decodeStdTiffEntry(object);
        if ( object->pValue()->count() < 3 || object->pValue()->typeId() != unsignedShort ) return; // insufficient data

        // create vector of signedShorts from unsignedShorts in Exif.Canon.AFInfo
        std::vector<int16_t>  ints;
        std::vector<uint16_t> uint;
        for (size_t i = 0; i < object->pValue()->count(); i++) {
            ints.push_back(static_cast<int16_t>(object->pValue()->toInt64(i)));
            uint.push_back(static_cast<uint16_t>(object->pValue()->toInt64(i)));
        }
        // Check this is AFInfo2 (ints[0] = bytes in object)
        if ( ints.at(0) != static_cast<int16_t>(object->pValue()->count())*2 )
            return ;

        std::string familyGroup(std::string("Exif.") + groupName(object->group()) + ".");

        const uint16_t nPoints = uint.at(2);
        const uint16_t nMasks  = (nPoints+15)/(sizeof(uint16_t) * 8);
        int            nStart  = 0;

        static const struct
        {
            uint16_t tag    ;
            uint16_t size   ;
            bool     bSigned;
        } records[] = {
            {0x2600, 1, true},        // AFInfoSize
            {0x2601, 1, true},        // AFAreaMode
            {0x2602, 1, true},        // AFNumPoints
            {0x2603, 1, true},        // AFValidPoints
            {0x2604, 1, true},        // AFCanonImageWidth
            {0x2605, 1, true},        // AFCanonImageHeight
            {0x2606, 1, true},        // AFImageWidth"
            {0x2607, 1, true},        // AFImageHeight
            {0x2608, nPoints, true},  // AFAreaWidths
            {0x2609, nPoints, true},  // AFAreaHeights
            {0x260a, nPoints, true},  // AFXPositions
            {0x260b, nPoints, true},  // AFYPositions
            {0x260c, nMasks, false},  // AFPointsInFocus
            {0x260d, nMasks, false},  // AFPointsSelected
            {0x260e, nMasks, false},  // AFPointsUnusable
        };
        // check we have enough data!
        uint16_t count = 0;
        for (auto&& record : records) {
            count += record.size;
            if (count > ints.size())
                return;
        }

        for (auto&& record : records) {
            const TagInfo* pTags = ExifTags::tagList("Canon") ;
            const TagInfo* pTag = findTag(pTags, record.tag);
            if ( pTag ) {
                auto v = Exiv2::Value::create(record.bSigned ? Exiv2::signedShort : Exiv2::unsignedShort);
                std::ostringstream s;
                if (record.bSigned) {
                    for (uint16_t k = 0; k < record.size; k++)
                        s << " " << ints.at(nStart++);
                } else {
                    for (uint16_t k = 0; k < record.size; k++)
                        s << " " << uint.at(nStart++);
                }

                v->read(s.str());
                exifData_[familyGroup + pTag->name_] = *v;
            }
        }
    }

    void TiffDecoder::decodeTiffEntry(const TiffEntryBase* object)
    {
        assert(object);

        // Don't decode the entry if value is not set
        if (!object->pValue()) return;

        const DecoderFct decoderFct = findDecoderFct_(make_,
                                                      object->tag(),
                                                      object->group());
        // skip decoding if decoderFct == 0
        if (decoderFct) {
            EXV_CALL_MEMBER_FN(*this, decoderFct)(object);
        }
    } // TiffDecoder::decodeTiffEntry

    void TiffDecoder::decodeStdTiffEntry(const TiffEntryBase* object)
    {
        assert(object);
        ExifKey key(object->tag(), groupName(object->group()));
        key.setIdx(object->idx());
        exifData_.add(key, object->pValue());

    } // TiffDecoder::decodeTiffEntry

    void TiffDecoder::visitBinaryArray(TiffBinaryArray* object)
    {
        if (!object->cfg() || !object->decoded()) {
            decodeTiffEntry(object);
        }
    }

    void TiffDecoder::visitBinaryElement(TiffBinaryElement* object)
    {
        decodeTiffEntry(object);
    }

    TiffEncoder::TiffEncoder(ExifData exifData, const IptcData& iptcData, const XmpData& xmpData, TiffComponent* pRoot,
                             const bool isNewImage, const PrimaryGroups* pPrimaryGroups, const TiffHeaderBase* pHeader,
                             FindEncoderFct findEncoderFct)
        : exifData_(std::move(exifData)),
          iptcData_(iptcData),
          xmpData_(xmpData),
          del_(true),
          pHeader_(pHeader),
          pRoot_(pRoot),
          isNewImage_(isNewImage),
          pPrimaryGroups_(pPrimaryGroups),
          pSourceTree_(nullptr),
          findEncoderFct_(findEncoderFct),
          dirty_(false),
          writeMethod_(wmNonIntrusive)
    {
        assert(pRoot);
        assert(pPrimaryGroups);
        assert(pHeader);

        byteOrder_ = pHeader->byteOrder();
        origByteOrder_ = byteOrder_;

        encodeIptc();
        encodeXmp();

        // Find camera make
        ExifKey key("Exif.Image.Make");
        auto pos = exifData_.findKey(key);
        if (pos != exifData_.end()) {
            make_ = pos->toString();
        }
        if (make_.empty() && pRoot_) {
            TiffFinder finder(0x010f, ifd0Id);
            pRoot_->accept(finder);
            auto te = dynamic_cast<TiffEntryBase*>(finder.result());
            if (te && te->pValue()) {
                make_ = te->pValue()->toString();
            }
        }
    }

    void TiffEncoder::encodeIptc()
    {
        // Update IPTCNAA Exif tag, if it exists. Delete the tag if there
        // is no IPTC data anymore.
        // If there is new IPTC data and Exif.Image.ImageResources does
        // not exist, create a new IPTCNAA Exif tag.
        bool del = false;
        ExifKey iptcNaaKey("Exif.Image.IPTCNAA");
        auto pos = exifData_.findKey(iptcNaaKey);
        if (pos != exifData_.end()) {
            iptcNaaKey.setIdx(pos->idx());
            exifData_.erase(pos);
            del = true;
        }
        DataBuf rawIptc = IptcParser::encode(iptcData_);
        ExifKey irbKey("Exif.Image.ImageResources");
        pos = exifData_.findKey(irbKey);
        if (pos != exifData_.end()) {
            irbKey.setIdx(pos->idx());
        }
        if (!rawIptc.empty() && (del || pos == exifData_.end())) {
            auto value = Value::create(unsignedLong);
            DataBuf buf;
            if (rawIptc.size() % 4 != 0) {
                // Pad the last unsignedLong value with 0s
                buf.alloc((rawIptc.size() / 4) * 4 + 4);
                buf.copyBytes(0, rawIptc.c_data(), rawIptc.size());
            }
            else {
                buf = std::move(rawIptc); // Note: This resets rawIptc
            }
            value->read(buf.data(), static_cast<long>(buf.size()), byteOrder_);
            Exifdatum iptcDatum(iptcNaaKey, value.get());
            exifData_.add(iptcDatum);
            pos = exifData_.findKey(irbKey); // needed after add()
        }
        // Also update IPTC IRB in Exif.Image.ImageResources if it exists,
        // but don't create it if not.
        if (pos != exifData_.end()) {
            DataBuf irbBuf(pos->value().size());
            pos->value().copy(irbBuf.data(), invalidByteOrder);
            irbBuf = Photoshop::setIptcIrb(irbBuf.c_data(), irbBuf.size(), iptcData_);
            exifData_.erase(pos);
            if (!irbBuf.empty()) {
                auto value = Value::create(unsignedByte);
                value->read(irbBuf.data(), static_cast<long>(irbBuf.size()), invalidByteOrder);
                Exifdatum iptcDatum(irbKey, value.get());
                exifData_.add(iptcDatum);
            }
        }
    } // TiffEncoder::encodeIptc

    void TiffEncoder::encodeXmp()
    {
#ifdef EXV_HAVE_XMP_TOOLKIT
        ExifKey xmpKey("Exif.Image.XMLPacket");
        // Remove any existing XMP Exif tag
        auto pos = exifData_.findKey(xmpKey);
        if (pos != exifData_.end()) {
            xmpKey.setIdx(pos->idx());
            exifData_.erase(pos);
        }
        std::string xmpPacket;
        if ( xmpData_.usePacket() ) {
            xmpPacket = xmpData_.xmpPacket();
        } else {
            if (XmpParser::encode(xmpPacket, xmpData_) > 1) {
#ifndef SUPPRESS_WARNINGS
                EXV_ERROR << "Failed to encode XMP metadata.\n";
#endif
            }
        }
        if (!xmpPacket.empty()) {
            // Set the XMP Exif tag to the new value
            auto value = Value::create(unsignedByte);
            value->read(reinterpret_cast<const byte*>(&xmpPacket[0]),
                        static_cast<long>(xmpPacket.size()),
                        invalidByteOrder);
            Exifdatum xmpDatum(xmpKey, value.get());
            exifData_.add(xmpDatum);
        }
#endif
    } // TiffEncoder::encodeXmp

    void TiffEncoder::setDirty(bool flag)
    {
        dirty_ = flag;
        setGo(geTraverse, !flag);
    }

    bool TiffEncoder::dirty() const { return dirty_ || !exifData_.empty(); }

    void TiffEncoder::visitEntry(TiffEntry* object)
    {
        encodeTiffComponent(object);
    }

    void TiffEncoder::visitDataEntry(TiffDataEntry* object)
    {
        encodeTiffComponent(object);
    }

    void TiffEncoder::visitImageEntry(TiffImageEntry* object)
    {
        encodeTiffComponent(object);
    }

    void TiffEncoder::visitSizeEntry(TiffSizeEntry* object)
    {
        encodeTiffComponent(object);
    }

    void TiffEncoder::visitDirectory(TiffDirectory* /*object*/)
    {
        // Nothing to do
    }

    void TiffEncoder::visitDirectoryNext(TiffDirectory* object)
    {
        // Update type and count in IFD entries, in case they changed
        assert(object);

        byte* p = object->start() + 2;
        for (auto&& component : object->components_) {
            p += updateDirEntry(p, byteOrder(), component);
        }
    }

    uint32_t TiffEncoder::updateDirEntry(byte* buf, ByteOrder byteOrder, TiffComponent* pTiffComponent)
    {
        assert(buf);
        assert(pTiffComponent);
        auto pTiffEntry = dynamic_cast<TiffEntryBase*>(pTiffComponent);
        assert(pTiffEntry);
        us2Data(buf + 2, pTiffEntry->tiffType(), byteOrder);
        ul2Data(buf + 4, static_cast<uint32_t>(pTiffEntry->count()), byteOrder);
        // Move data to offset field, if it fits and is not yet there.
        if (pTiffEntry->size() <= 4 && buf + 8 != pTiffEntry->pData()) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cerr << "Copying data for tag " << pTiffEntry->tag()
                      << " to offset area.\n";
#endif
            memset(buf + 8, 0x0, 4);
            if (pTiffEntry->size() > 0) {
                memmove(buf + 8, pTiffEntry->pData(), pTiffEntry->size());
                memset(const_cast<byte*>(pTiffEntry->pData()), 0x0, pTiffEntry->size());
            }
        }
        return 12;
    }

    void TiffEncoder::visitSubIfd(TiffSubIfd* object)
    {
        encodeTiffComponent(object);
    }

    void TiffEncoder::visitMnEntry(TiffMnEntry* object)
    {
        // Test is required here as well as in the callback encoder function
        if (!object->mn_) {
            encodeTiffComponent(object);
        }
        else if (del_) {
            // The makernote is made up of decoded tags, delete binary tag
            ExifKey key(object->tag(), groupName(object->group()));
            auto pos = exifData_.findKey(key);
            if (pos != exifData_.end()) exifData_.erase(pos);
        }
    }

    void TiffEncoder::visitIfdMakernote(TiffIfdMakernote* object)
    {
        assert(object);

        auto pos = exifData_.findKey(ExifKey("Exif.MakerNote.ByteOrder"));
        if (pos != exifData_.end()) {
            // Set Makernote byte order
            ByteOrder bo = stringToByteOrder(pos->toString());
            if (bo != invalidByteOrder && bo != object->byteOrder()) {
                object->setByteOrder(bo);
                setDirty();
            }
            if (del_) exifData_.erase(pos);
        }
        if (del_) {
            // Remove remaining synthesized tags
            static constexpr auto synthesizedTags = std::array{
                "Exif.MakerNote.Offset",
            };
            for (auto&& synthesizedTag : synthesizedTags) {
                pos = exifData_.findKey(ExifKey(synthesizedTag));
                if (pos != exifData_.end()) exifData_.erase(pos);
            }
        }
        // Modify encoder for Makernote peculiarities, byte order
        byteOrder_ = object->byteOrder();

    } // TiffEncoder::visitIfdMakernote

    void TiffEncoder::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/)
    {
        // Reset byte order back to that from the c'tor
        byteOrder_ = origByteOrder_;

    } // TiffEncoder::visitIfdMakernoteEnd

    void TiffEncoder::visitBinaryArray(TiffBinaryArray* object)
    {
        if (!object->cfg() || !object->decoded()) {
            encodeTiffComponent(object);
        }
    }

    void TiffEncoder::visitBinaryArrayEnd(TiffBinaryArray* object)
    {
        assert(object);

        if (!object->cfg() || !object->decoded())
            return;
        size_t size = object->TiffEntryBase::doSize();
        if (size == 0)
            return;
        if (!object->initialize(pRoot_))
            return;

        // Re-encrypt buffer if necessary
        CryptFct cryptFct = object->cfg()->cryptFct_;
        if ( cryptFct == sonyTagDecipher ) {
             cryptFct  = sonyTagEncipher;
        }
        if (cryptFct) {
            const byte* pData = object->pData();
            DataBuf buf = cryptFct(object->tag(), pData, size, pRoot_);
            if (!buf.empty()) {
                pData = buf.c_data();
                size = static_cast<int32_t>(buf.size());
            }
            if (!object->updOrigDataBuf(pData, size)) {
                setDirty();
            }
        }
    }

    void TiffEncoder::visitBinaryElement(TiffBinaryElement* object)
    {
        // Temporarily overwrite byte order according to that of the binary element
        ByteOrder boOrig = byteOrder_;
        if (object->elByteOrder() != invalidByteOrder) byteOrder_ = object->elByteOrder();
        encodeTiffComponent(object);
        byteOrder_ = boOrig;
    }

    bool TiffEncoder::isImageTag(uint16_t tag, IfdId group) const
    {
        return !isNewImage_ && pHeader_->isImageTag(tag, group, pPrimaryGroups_);
    }

    void TiffEncoder::encodeTiffComponent(
              TiffEntryBase* object,
        const Exifdatum*     datum
    )
    {
        assert(object);

        auto pos = exifData_.end();
        const Exifdatum* ed = datum;
        if (!ed) {
            // Non-intrusive writing: find matching tag
            ExifKey key(object->tag(), groupName(object->group()));
            pos = exifData_.findKey(key);
            if (pos != exifData_.end()) {
                ed = &(*pos);
                if (object->idx() != pos->idx()) {
                    // Try to find exact match (in case of duplicate tags)
                    auto pos2 =
                        std::find_if(exifData_.begin(), exifData_.end(),
                                     FindExifdatum2(object->group(), object->idx()));
                    if (pos2 != exifData_.end() && pos2->key() == key.key()) {
                        ed = &(*pos2);
                        pos = pos2; // make sure we delete the correct tag below
                    }
                }
            }
            else {
                setDirty();
#ifdef EXIV2_DEBUG_MESSAGES
                std::cerr << "DELETING          " << key << ", idx = " << object->idx() << "\n";
#endif
            }
        } else {
            // For intrusive writing, the index is used to preserve the order of
            // duplicate tags
            object->idx_ = ed->idx();
        }
        // Skip encoding image tags of existing TIFF image - they were copied earlier -
        // but encode image tags of new images (creation)
        if (ed && !isImageTag(object->tag(), object->group())) {
            const EncoderFct fct = findEncoderFct_(make_, object->tag(), object->group());
            if (fct) {
                // If an encoding function is registered for the tag, use it
                EXV_CALL_MEMBER_FN(*this, fct)(object, ed);
            }
            else {
                // Else use the encode function at the object (results in a double-dispatch
                // to the appropriate encoding function of the encoder.
                object->encode(*this, ed);
            }
        }
        if (del_ && pos != exifData_.end()) {
            exifData_.erase(pos);
        }
#ifdef EXIV2_DEBUG_MESSAGES
        std::cerr << "\n";
#endif
    } // TiffEncoder::encodeTiffComponent

    void TiffEncoder::encodeBinaryArray(TiffBinaryArray* object, const Exifdatum* datum)
    {
        encodeOffsetEntry(object, datum);
    } // TiffEncoder::encodeBinaryArray

    void TiffEncoder::encodeBinaryElement(TiffBinaryElement* object, const Exifdatum* datum)
    {
        encodeTiffEntryBase(object, datum);
    } // TiffEncoder::encodeArrayElement

    void TiffEncoder::encodeDataEntry(TiffDataEntry* object, const Exifdatum* datum)
    {
        encodeOffsetEntry(object, datum);

        if (!dirty_ && writeMethod() == wmNonIntrusive) {
            assert(object);
            assert(object->pValue());
            if (  object->sizeDataArea_
                < static_cast<uint32_t>(object->pValue()->sizeDataArea())) {
#ifdef EXIV2_DEBUG_MESSAGES
                ExifKey key(object->tag(), groupName(object->group()));
                std::cerr << "DATAAREA GREW     " << key << "\n";
#endif
                setDirty();
            }
            else {
                // Write the new dataarea, fill with 0x0
#ifdef EXIV2_DEBUG_MESSAGES
                ExifKey key(object->tag(), groupName(object->group()));
                std::cerr << "Writing data area for " << key << "\n";
#endif
                DataBuf buf = object->pValue()->dataArea();
                if (!buf.empty()) {
                    memcpy(object->pDataArea_, buf.c_data(), buf.size());
                    if (object->sizeDataArea_ > static_cast<size_t>(buf.size())) {
                        memset(object->pDataArea_ + buf.size(),
                           0x0, object->sizeDataArea_ - buf.size());
                    }
                }
            }
        }

    } // TiffEncoder::encodeDataEntry

    void TiffEncoder::encodeTiffEntry(TiffEntry* object, const Exifdatum* datum)
    {
        encodeTiffEntryBase(object, datum);
    } // TiffEncoder::encodeTiffEntry

    void TiffEncoder::encodeImageEntry(TiffImageEntry* object, const Exifdatum* datum)
    {
        encodeOffsetEntry(object, datum);

        size_t sizeDataArea = object->pValue()->sizeDataArea();

        if (sizeDataArea > 0 && writeMethod() == wmNonIntrusive) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cerr << "\t DATAAREA IS SET (NON-INTRUSIVE WRITING)";
#endif
            setDirty();
        }

        if (sizeDataArea > 0 && writeMethod() == wmIntrusive) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cerr << "\t DATAAREA IS SET (INTRUSIVE WRITING)";
#endif
            // Set pseudo strips (without a data pointer) from the size tag
            ExifKey key(object->szTag(), groupName(object->szGroup()));
            auto pos = exifData_.findKey(key);
            const byte* zero = nullptr;
            if (pos == exifData_.end()) {
#ifndef SUPPRESS_WARNINGS
                EXV_ERROR << "Size tag " << key
                          << " not found. Writing only one strip.\n";
#endif
                object->strips_.clear();
                object->strips_.emplace_back(zero, static_cast<uint32_t>(sizeDataArea));
            }
            else {
                uint32_t sizeTotal = 0;
                object->strips_.clear();
                for (size_t i = 0; i < pos->count(); ++i) {
                    uint32_t len = pos->toUint32(static_cast<long>(i));
                    object->strips_.emplace_back(zero, len);
                    sizeTotal += len;
                }
                if (sizeTotal != sizeDataArea) {
#ifndef SUPPRESS_WARNINGS
                    ExifKey key2(object->tag(), groupName(object->group()));
                    EXV_ERROR << "Sum of all sizes of " << key
                              << " != data size of " << key2 << ". "
                              << "This results in an invalid image.\n";
#endif
                    // Todo: How to fix? Write only one strip?
                }
            }
        }

        if (sizeDataArea == 0 && writeMethod() == wmIntrusive) {
#ifdef EXIV2_DEBUG_MESSAGES
            std::cerr << "\t USE STRIPS FROM SOURCE TREE IMAGE ENTRY";
#endif
            // Set strips from source tree
            if (pSourceTree_) {
                TiffFinder finder(object->tag(), object->group());
                pSourceTree_->accept(finder);
                auto ti = dynamic_cast<TiffImageEntry*>(finder.result());
                if (ti) {
                    object->strips_ = ti->strips_;
                }
            }
#ifndef SUPPRESS_WARNINGS
            else {
                ExifKey key2(object->tag(), groupName(object->group()));
                EXV_WARNING << "No image data to encode " << key2 << ".\n";
            }
#endif
        }

    } // TiffEncoder::encodeImageEntry

    void TiffEncoder::encodeMnEntry(TiffMnEntry* object, const Exifdatum* datum)
    {
        // Test is required here as well as in the visit function
        if (!object->mn_) encodeTiffEntryBase(object, datum);
    } // TiffEncoder::encodeMnEntry

    void TiffEncoder::encodeSizeEntry(TiffSizeEntry* object, const Exifdatum* datum)
    {
        encodeTiffEntryBase(object, datum);
    } // TiffEncoder::encodeSizeEntry

    void TiffEncoder::encodeSubIfd(TiffSubIfd* object, const Exifdatum* datum)
    {
        encodeOffsetEntry(object, datum);
    } // TiffEncoder::encodeSubIfd

    void TiffEncoder::encodeTiffEntryBase(TiffEntryBase* object, const Exifdatum* datum)
    {
        assert(object);
        assert(datum);

#ifdef EXIV2_DEBUG_MESSAGES
        bool tooLarge = false;
#endif
        size_t newSize = datum->size();
        if (newSize > object->size_) { // value doesn't fit, encode for intrusive writing
            setDirty();
#ifdef EXIV2_DEBUG_MESSAGES
            tooLarge = true;
#endif
        }
        object->updateValue(datum->getValue(), byteOrder()); // clones the value
#ifdef EXIV2_DEBUG_MESSAGES
        ExifKey key(object->tag(), groupName(object->group()));
        std::cerr << "UPDATING DATA     " << key;
        if (tooLarge) {
            std::cerr << "\t\t\t ALLOCATED " << std::dec << object->size_ << " BYTES";
        }
#endif
    }

    void TiffEncoder::encodeOffsetEntry(TiffEntryBase* object, const Exifdatum* datum)
    {
        assert(object);
        assert(datum);

        size_t newSize = datum->size();
        if (newSize > object->size_) { // value doesn't fit, encode for intrusive writing
            setDirty();
            object->updateValue(datum->getValue(), byteOrder()); // clones the value
#ifdef EXIV2_DEBUG_MESSAGES
            ExifKey key(object->tag(), groupName(object->group()));
            std::cerr << "UPDATING DATA     " << key;
            std::cerr << "\t\t\t ALLOCATED " << object->size() << " BYTES";
#endif
        }
        else {
            object->setValue(datum->getValue()); // clones the value
#ifdef EXIV2_DEBUG_MESSAGES
            ExifKey key(object->tag(), groupName(object->group()));
            std::cerr << "NOT UPDATING      " << key;
            std::cerr << "\t\t\t PRESERVE VALUE DATA";
#endif
        }
    }

    void TiffEncoder::add(
        TiffComponent* pRootDir,
        TiffComponent* pSourceDir,
        uint32_t       root
    )
    {
        assert(pRootDir);

        writeMethod_ = wmIntrusive;
        pSourceTree_ = pSourceDir;

        // Ensure that the exifData_ entries are not deleted, to be able to
        // iterate over all remaining entries.
        del_ = false;

        auto posBo = exifData_.end();
        for (auto i = exifData_.begin();
             i != exifData_.end(); ++i) {

            IfdId group = groupId(i->groupName());
            // Skip synthesized info tags
            if (group == mnId) {
                if (i->tag() == 0x0002) {
                    posBo = i;
                }
                continue;
            }

            // Skip image tags of existing TIFF image - they were copied earlier -
            // but add and encode image tags of new images (creation)
            if (isImageTag(i->tag(), group)) continue;

            // Assumption is that the corresponding TIFF entry doesn't exist
            TiffPath tiffPath;
            TiffCreator::getPath(tiffPath, i->tag(), group, root);
            TiffComponent* tc = pRootDir->addPath(i->tag(), tiffPath, pRootDir);
            auto object = dynamic_cast<TiffEntryBase*>(tc);
#ifdef EXIV2_DEBUG_MESSAGES
            if (object == 0) {
                std::cerr << "Warning: addPath() didn't add an entry for "
                          << i->groupName()
                          << " tag 0x" << std::setw(4) << std::setfill('0')
                          << std::hex << i->tag() << "\n";
            }
#endif
            if (object) {
                encodeTiffComponent(object, &(*i));
            }
        }

        /*
          What follows is a hack. I can't think of a better way to set
          the makernote byte order (and other properties maybe) in the
          makernote header during intrusive writing. The thing is that
          visit/encodeIfdMakernote is not called in this case and there
          can't be an Exif tag which corresponds to this component.
         */
        if (posBo == exifData_.end()) return;

        TiffFinder finder(0x927c, exifId);
        pRootDir->accept(finder);
        auto te = dynamic_cast<TiffMnEntry*>(finder.result());
        if (te) {
            auto tim = dynamic_cast<TiffIfdMakernote*>(te->mn_);
            if (tim) {
                // Set Makernote byte order
                ByteOrder bo = stringToByteOrder(posBo->toString());
                if (bo != invalidByteOrder) tim->setByteOrder(bo);
            }
        }

    } // TiffEncoder::add

    TiffReader::TiffReader(const byte* pData, size_t size, TiffComponent* pRoot, TiffRwState state)
        : pData_(pData),
          size_(size),
          pLast_(pData + size),
          pRoot_(pRoot),
          origState_(state),
          mnState_(state),
          postProc_(false)
    {
        pState_ = &origState_;
        assert(pData_);
        assert(size_);

    } // TiffReader::TiffReader

    void TiffReader::setOrigState()
    {
        pState_ = &origState_;
    }

    void TiffReader::setMnState(const TiffRwState* state)
    {
        if (state) {
            // invalidByteOrder indicates 'no change'
            if (state->byteOrder() == invalidByteOrder) {
                mnState_ = TiffRwState(origState_.byteOrder(), state->baseOffset());
            }
            else {
                mnState_ = *state;
            }
        }
        pState_ = &mnState_;
    }

    ByteOrder TiffReader::byteOrder() const
    {
        assert(pState_);
        return pState_->byteOrder();
    }

    uint32_t TiffReader::baseOffset() const
    {
        assert(pState_);
        return pState_->baseOffset();
    }

    void TiffReader::readDataEntryBase(TiffDataEntryBase* object)
    {
        assert(object);

        readTiffEntry(object);
        TiffFinder finder(object->szTag(), object->szGroup());
        pRoot_->accept(finder);
        auto te = dynamic_cast<TiffEntryBase*>(finder.result());
        if (te && te->pValue()) {
            object->setStrips(te->pValue(), pData_, size_, baseOffset());
        }
    }

    void TiffReader::visitEntry(TiffEntry* object)
    {
        readTiffEntry(object);
    }

    void TiffReader::visitDataEntry(TiffDataEntry* object)
    {
        readDataEntryBase(object);
    }

    void TiffReader::visitImageEntry(TiffImageEntry* object)
    {
        readDataEntryBase(object);
    }

    void TiffReader::visitSizeEntry(TiffSizeEntry* object)
    {
        assert(object);

        readTiffEntry(object);
        TiffFinder finder(object->dtTag(), object->dtGroup());
        pRoot_->accept(finder);
        auto te = dynamic_cast<TiffDataEntryBase*>(finder.result());
        if (te && te->pValue()) {
            te->setStrips(object->pValue(), pData_, size_, baseOffset());
        }
    }

    bool TiffReader::circularReference(const byte* start, IfdId group)
    {
        auto pos = dirList_.find(start);
        if (pos != dirList_.end()) {
#ifndef SUPPRESS_WARNINGS
            EXV_ERROR << groupName(group) << " pointer references previously read "
                      << groupName(pos->second) << " directory; ignored.\n";
#endif
            return true;
        }
        dirList_[start] = group;
        return false;
    }

    int TiffReader::nextIdx(IfdId group)
    {
        return ++idxSeq_[group];
    }

    void TiffReader::postProcess()
    {
        setMnState(); // All components to be post-processed must be from the Makernote
        postProc_ = true;
        for (auto&& pos : postList_) {
            pos->accept(*this);
        }
        postProc_ = false;
        setOrigState();
    }

    void TiffReader::visitDirectory(TiffDirectory* object)
    {
        assert(object);

        const byte* p = object->start();
        assert(p >= pData_);

        if (circularReference(object->start(), object->group())) return;

        if (p + 2 > pLast_) {
#ifndef SUPPRESS_WARNINGS
            EXV_ERROR << "Directory " << groupName(object->group())
                      << ": IFD exceeds data buffer, cannot read entry count.\n";
#endif
            return;
        }
        const uint16_t n = getUShort(p, byteOrder());
        p += 2;
        // Sanity check with an "unreasonably" large number
        if (n > 256) {
#ifndef SUPPRESS_WARNINGS
            EXV_ERROR << "Directory " << groupName(object->group()) << " with "
                      << n << " entries considered invalid; not read.\n";
#endif
            return;
        }
        for (uint16_t i = 0; i < n; ++i) {
            if (p + 12 > pLast_) {
#ifndef SUPPRESS_WARNINGS
                EXV_ERROR << "Directory " << groupName(object->group())
                          << ": IFD entry " << i
                          << " lies outside of the data buffer.\n";
#endif
                return;
            }
            uint16_t tag = getUShort(p, byteOrder());
            auto tc = TiffCreator::create(tag, object->group());
            if (tc) {
                tc->setStart(p);
                object->addChild(std::move(tc));
            } else {
#ifndef SUPPRESS_WARNINGS
               EXV_WARNING << "Unable to handle tag " << tag << ".\n";
#endif
            }
            p += 12;
        }

        if (object->hasNext()) {
            if (p + 4 > pLast_) {
#ifndef SUPPRESS_WARNINGS
                EXV_ERROR << "Directory " << groupName(object->group())
                          << ": IFD exceeds data buffer, cannot read next pointer.\n";
#endif
                return;
            }
            TiffComponent::UniquePtr tc;
            uint32_t next = getLong(p, byteOrder());
            if (next) {
                tc = TiffCreator::create(Tag::next, object->group());
#ifndef SUPPRESS_WARNINGS
                if (!tc) {
                    EXV_WARNING << "Directory " << groupName(object->group())
                                << " has an unexpected next pointer; ignored.\n";
                }
#endif
            }
            if (tc) {
                if (baseOffset() + next > size_) {
#ifndef SUPPRESS_WARNINGS
                    EXV_ERROR << "Directory " << groupName(object->group())
                              << ": Next pointer is out of bounds; ignored.\n";
#endif
                    return;
                }
                tc->setStart(pData_ + baseOffset() + next);
                object->addNext(std::move(tc));
            }
        } // object->hasNext()

    } // TiffReader::visitDirectory

    void TiffReader::visitSubIfd(TiffSubIfd* object)
    {
        assert(object);

        readTiffEntry(object);
        if (   (object->tiffType() == ttUnsignedLong || object->tiffType() == ttSignedLong
                || object->tiffType() == ttTiffIfd)
            && object->count() >= 1) {
            // Todo: Fix hack
            uint32_t maxi = 9;
            if (object->group() == ifd1Id) maxi = 1;
            for (uint32_t i = 0; i < object->count(); ++i) {
                uint32_t offset = getLong(object->pData() + 4*i, byteOrder());
                if (   baseOffset() + offset > size_ ) {
#ifndef SUPPRESS_WARNINGS
                    EXV_ERROR << "Directory " << groupName(object->group())
                              << ", entry 0x" << std::setw(4)
                              << std::setfill('0') << std::hex << object->tag()
                              << " Sub-IFD pointer " << i
                              << " is out of bounds; ignoring it.\n";
#endif
                    return;
                }
                if (i >= maxi) {
#ifndef SUPPRESS_WARNINGS
                    EXV_WARNING << "Directory " << groupName(object->group())
                                << ", entry 0x" << std::setw(4)
                                << std::setfill('0') << std::hex << object->tag()
                                << ": Skipping sub-IFDs beyond the first " << i << ".\n";
#endif
                    break;
                }
                // If there are multiple dirs, group is incremented for each
                auto td = std::make_unique<TiffDirectory>(object->tag(),
                                                            static_cast<IfdId>(object->newGroup_ + i));
                td->setStart(pData_ + baseOffset() + offset);
                object->addChild(std::move(td));
            }
        }
#ifndef SUPPRESS_WARNINGS
        else {
            EXV_WARNING << "Directory " << groupName(object->group())
                        << ", entry 0x" << std::setw(4)
                        << std::setfill('0') << std::hex << object->tag()
                        << " doesn't look like a sub-IFD.\n";
        }
#endif

    } // TiffReader::visitSubIfd

    void TiffReader::visitMnEntry(TiffMnEntry* object)
    {
        assert(object);

        readTiffEntry(object);
        // Find camera make
        TiffFinder finder(0x010f, ifd0Id);
        pRoot_->accept(finder);
        auto te = dynamic_cast<TiffEntryBase*>(finder.result());
        std::string make;
        if (te && te->pValue()) {
            make = te->pValue()->toString();
            // create concrete makernote, based on make and makernote contents
            object->mn_ = TiffMnCreator::create(object->tag(),
                                                object->mnGroup_,
                                                make,
                                                object->pData_,
                                                object->size_,
                                                byteOrder());
        }
        if (object->mn_) object->mn_->setStart(object->pData());

    } // TiffReader::visitMnEntry

    void TiffReader::visitIfdMakernote(TiffIfdMakernote* object)
    {
        assert(object);

        object->setImageByteOrder(byteOrder()); // set the byte order for the image

        if (!object->readHeader(object->start(),
                                static_cast<uint32_t>(pLast_ - object->start()),
                                byteOrder())) {
#ifndef SUPPRESS_WARNINGS
            EXV_ERROR << "Failed to read "
                      << groupName(object->ifd_.group())
                      << " IFD Makernote header.\n";
#ifdef EXIV2_DEBUG_MESSAGES
            if (static_cast<uint32_t>(pLast_ - object->start()) >= 16) {
                hexdump(std::cerr, object->start(), 16);
            }
#endif // EXIV2_DEBUG_MESSAGES
#endif // SUPPRESS_WARNINGS
            setGo(geKnownMakernote, false);
            return;
        }

        object->ifd_.setStart(object->start() + object->ifdOffset());

        // Modify reader for Makernote peculiarities, byte order and offset
        object->mnOffset_ = static_cast<uint32_t>(object->start() - pData_);
        TiffRwState state(object->byteOrder(), object->baseOffset());
        setMnState(&state);

    } // TiffReader::visitIfdMakernote

    void TiffReader::visitIfdMakernoteEnd(TiffIfdMakernote* /*object*/)
    {
        // Reset state (byte order, create function, offset) back to that for the image
        setOrigState();
    } // TiffReader::visitIfdMakernoteEnd

    void TiffReader::readTiffEntry(TiffEntryBase* object)
    {
        assert(object);

        byte* p = object->start();
        assert(p >= pData_);

        if (p + 12 > pLast_) {
#ifndef SUPPRESS_WARNINGS
            EXV_ERROR << "Entry in directory " << groupName(object->group())
                      << "requests access to memory beyond the data buffer. "
                      << "Skipping entry.\n";
#endif
            return;
        }
        // Component already has tag
        p += 2;
        TiffType tiffType = getUShort(p, byteOrder());
        TypeId typeId = toTypeId(tiffType, object->tag(), object->group());
        size_t typeSize = TypeInfo::typeSize(typeId);
        if (0 == typeSize) {
#ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Directory " << groupName(object->group())
                        << ", entry 0x" << std::setw(4)
                        << std::setfill('0') << std::hex << object->tag()
                        << " has unknown Exif (TIFF) type " << std::dec << tiffType
                        << "; setting type size 1.\n";
#endif
            typeSize = 1;
        }
        p += 2;
        uint32_t count = getULong(p, byteOrder());
        if (count >= 0x10000000) {
#ifndef SUPPRESS_WARNINGS
            EXV_ERROR << "Directory " << groupName(object->group())
                      << ", entry 0x" << std::setw(4)
                      << std::setfill('0') << std::hex << object->tag()
                      << " has invalid size "
                      << std::dec << count << "*" << typeSize
                      << "; skipping entry.\n";
#endif
            return;
        }
        p += 4;

        if (count > std::numeric_limits<uint32_t>::max() / typeSize) {
            throw Error(ErrorCode::kerArithmeticOverflow);
        }
        auto size = static_cast<uint32_t>(typeSize * count);
        uint32_t offset = getLong(p, byteOrder());
        byte* pData = p;
        if (   size > 4
            && (   baseOffset() + offset >= size_
                || static_cast<int32_t>(baseOffset()) + offset <= 0)) {
                // #1143
                if ( object->tag() == 0x2001 && std::string(groupName(object->group())) == "Sony1" ) {
                    // This tag is Exif.Sony1.PreviewImage, which refers to a preview image which is
                    // not stored in the metadata. Instead it is stored at the end of the file, after
                    // the main image. The value of `size` refers to the size of the preview image, not
                    // the size of the tag's payload, so we set it to zero here so that we don't attempt
                    // to read those bytes from the metadata. We currently leave this tag as "undefined",
                    // although we may attempt to handle it better in the future. More discussion of
                    // this issue can be found here:
                    //
                    //   https://github.com/Exiv2/exiv2/issues/2001
                    //   https://github.com/Exiv2/exiv2/pull/2008
                    //   https://github.com/Exiv2/exiv2/pull/2013
                    typeId = undefined;
                    size = 0;
                } else {
#ifndef SUPPRESS_WARNINGS
            EXV_ERROR << "Offset of directory " << groupName(object->group())
                      << ", entry 0x" << std::setw(4)
                      << std::setfill('0') << std::hex << object->tag()
                      << " is out of bounds: "
                      << "Offset = 0x" << std::setw(8)
                      << std::setfill('0') << std::hex << offset
                      << "; truncating the entry\n";
#endif
                }
                size = 0;
        }
        if (size > 4) {
            // setting pData to pData_ + baseOffset() + offset can result in pData pointing to invalid memory,
            // as offset can be arbitrarily large
            if ((static_cast<uintptr_t>(baseOffset()) > std::numeric_limits<uintptr_t>::max() - static_cast<uintptr_t>(offset))
             || (static_cast<uintptr_t>(baseOffset() + offset) > std::numeric_limits<uintptr_t>::max() - reinterpret_cast<uintptr_t>(pData_)))
            {
                throw Error(ErrorCode::kerCorruptedMetadata); // #562 don't throw kerArithmeticOverflow
            }
            if (pData_ + static_cast<uintptr_t>(baseOffset()) + static_cast<uintptr_t>(offset) > pLast_) {
                throw Error(ErrorCode::kerCorruptedMetadata);
            }
            pData = const_cast<byte*>(pData_) + baseOffset() + offset;

        // check for size being invalid
            if (size > static_cast<uint32_t>(pLast_ - pData)) {
#ifndef SUPPRESS_WARNINGS
                EXV_ERROR << "Upper boundary of data for "
                          << "directory " << groupName(object->group())
                          << ", entry 0x" << std::setw(4)
                          << std::setfill('0') << std::hex << object->tag()
                          << " is out of bounds: "
                          << "Offset = 0x" << std::setw(8)
                          << std::setfill('0') << std::hex << offset
                          << ", size = " << std::dec << size
                          << ", exceeds buffer size by "
                          // cast to make MSVC happy
                          << static_cast<uint32_t>(pData + size - pLast_)
                          << " Bytes; truncating the entry\n";
#endif
                size = 0;
            }
        }
        auto v = Value::create(typeId);
        enforce(v != nullptr, ErrorCode::kerCorruptedMetadata);
        v->read(pData, size, byteOrder());

        object->setValue(std::move(v));
        object->setData(pData, size, std::shared_ptr<DataBuf>());
        object->setOffset(offset);
        object->setIdx(nextIdx(object->group()));

    } // TiffReader::readTiffEntry

    void TiffReader::visitBinaryArray(TiffBinaryArray* object)
    {
        assert(object);

        if (!postProc_) {
            // Defer reading children until after all other components are read, but
            // since state (offset) is not set during post-processing, read entry here
            readTiffEntry(object);
            object->iniOrigDataBuf();
            postList_.push_back(object);
            return;
        }
        // Check duplicates
        TiffFinder finder(object->tag(), object->group());
        pRoot_->accept(finder);
        auto te = dynamic_cast<TiffEntryBase*>(finder.result());
        if (te && te->idx() != object->idx()) {
#ifndef SUPPRESS_WARNINGS
            EXV_WARNING << "Not decoding duplicate binary array tag 0x"
                        << std::setw(4) << std::setfill('0') << std::hex
                        << object->tag() << std::dec << ", group "
                        << groupName(object->group()) << ", idx " << object->idx()
                        << "\n";
#endif
            object->setDecoded(false);
            return;
        }

        if (object->TiffEntryBase::doSize() == 0) return;
        if (!object->initialize(pRoot_)) return;
        const ArrayCfg* cfg = object->cfg();
        if (!cfg)
            return;

        const CryptFct cryptFct = cfg->cryptFct_;
        if (cryptFct) {
            const byte* pData = object->pData();
            size_t size = object->TiffEntryBase::doSize();
            auto buf = std::make_shared<DataBuf>(cryptFct(object->tag(), pData, size, pRoot_));
            if (!buf->empty())
                object->setData(buf);
        }

        const ArrayDef* defs = object->def();
        const ArrayDef* defsEnd = defs + object->defSize();
        const ArrayDef* def = &cfg->elDefaultDef_;
        ArrayDef gap = *def;

        for (uint32_t idx = 0; idx < object->TiffEntryBase::doSize(); ) {
            if (defs) {
                def = std::find(defs, defsEnd, idx);
                if (def == defsEnd) {
                    if (cfg->concat_) {
                        // Determine gap-size
                        const ArrayDef* xdef = defs;
                        for (; xdef != defsEnd && xdef->idx_ <= idx; ++xdef) {}
                        uint32_t gapSize = 0;
                        if (xdef != defsEnd && xdef->idx_ > idx) {
                            gapSize = xdef->idx_ - idx;
                        }
                        else {
                            gapSize = static_cast<uint32_t>(object->TiffEntryBase::doSize()) - idx;
                        }
                        gap.idx_ = idx;
                        gap.tiffType_ = cfg->elDefaultDef_.tiffType_;
                        gap.count_ = gapSize / cfg->tagStep();
                        if (gap.count_ * cfg->tagStep() != gapSize) {
                            gap.tiffType_ = ttUndefined;
                            gap.count_ = gapSize;
                        }
                        def = &gap;
                    }
                    else {
                        def = &cfg->elDefaultDef_;
                    }
                }
            }
            idx += object->addElement(idx, *def); // idx may be different from def->idx_
        }

    } // TiffReader::visitBinaryArray

    void TiffReader::visitBinaryElement(TiffBinaryElement* object)
    {
        byte* pData   = object->start();
        size_t size = object->TiffEntryBase::doSize();
        ByteOrder bo = object->elByteOrder();
        if (bo == invalidByteOrder)
            bo = byteOrder();
        TypeId typeId = toTypeId(object->elDef()->tiffType_, object->tag(), object->group());
        auto v = Value::create(typeId);
        enforce(v != nullptr, ErrorCode::kerCorruptedMetadata);
        v->read(pData, size, bo);

        object->setValue(std::move(v));
        object->setOffset(0);
        object->setIdx(nextIdx(object->group()));
    }

}  // namespace Exiv2::Internal
