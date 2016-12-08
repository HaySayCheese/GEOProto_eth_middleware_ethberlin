#include "UUIDMapBlockStorage.h"

namespace db {
    namespace uuid_map_block_storage {

        UUIDMapBlockStorage::UUIDMapBlockStorage(const string fileName) {
            mFileName = fileName;
            obtainFileDescriptor();
            readFileHeader();
        }

        UUIDMapBlockStorage::~UUIDMapBlockStorage() {
            if (mFileDescriptor != NULL) {
                fclose(mFileDescriptor);
            }
        }

        void UUIDMapBlockStorage::write(const NodeUUID &uuid, const byte *block, const size_t blockBytesCount) {
            if (isUUIDTheIndex(uuid)){
                throw ConflictError(string("Unable to write data. Index with such uuid already exist. Maybe you should use rewrite() method.").c_str());
            }
            long offset = writeData(block, blockBytesCount);
            pair<uint32_t, uint64_t> fileHeaderData = writeIndexRecordsInMemory(uuid, offset, blockBytesCount);
            mMapIndexOffset = fileHeaderData.first;
            mMapIndexRecordsCount = fileHeaderData.second;
            writeFileHeader();
        }

        void UUIDMapBlockStorage::erase(const NodeUUID &uuid) {
            if (isUUIDTheIndex(uuid)){
                map<NodeUUID, pair<uint32_t, uint64_t>>::iterator seeker = mIndexBlock.find(uuid);
                if (seeker != mIndexBlock.end()) {
                    mIndexBlock.erase(seeker);
                    mMapIndexRecordsCount = (uint64_t) mIndexBlock.size();
                    fseek(mFileDescriptor, 0, SEEK_END);
                    mMapIndexOffset = (uint32_t) ftell(mFileDescriptor);
                    writeIndexBlock();
                    writeFileHeader();
                } else {
                    throw IndexError("Can't iterate map to find value by such key.");
                }
            } else {
                throw IndexError("Can't find such uuid in index block.");
            }
        }

        Block UUIDMapBlockStorage::readFromFile(const NodeUUID &uuid) {
            if (isUUIDTheIndex(uuid)){
                map<NodeUUID, pair<uint32_t, uint64_t>>::iterator seeker = mIndexBlock.find(uuid);
                if (seeker != mIndexBlock.end()) {
                    size_t offset = (size_t) seeker->second.first;
                    size_t bytesCount = (size_t) seeker->second.second;
                    fseek(mFileDescriptor, offset, SEEK_SET);
                    byte *dataBuffer = (byte *)malloc(bytesCount);
                    memset(dataBuffer, 0, bytesCount);
                    fread(dataBuffer, 1, bytesCount, mFileDescriptor);
                    Block block(dataBuffer, bytesCount);
                    block.mData = dataBuffer;
                    block.mBytesCount = bytesCount;
                    return block;
                } else {
                    throw IndexError("Can't iterate map to find value by such key.");
                }
            } else {
                throw IndexError("Can't find such uuid in index block.");
            }
        }

        const vector <NodeUUID> UUIDMapBlockStorage::keys() const {
            vector<NodeUUID> uuidsVector(mIndexBlock.size());
            for (auto &it : mIndexBlock){
                uuidsVector.push_back(it.first);
            }
            return uuidsVector;
        }

        void UUIDMapBlockStorage::vacuum() {
            UUIDMapBlockStorage *mapBlockStorage = new UUIDMapBlockStorage(kTempFileName);
            map<NodeUUID, pair<uint32_t, uint64_t>>::iterator looper;
            for (looper = mIndexBlock.begin(); looper != mIndexBlock.end(); ++looper){
                Block block = readFromFile(looper->first);
                mapBlockStorage->write(looper->first, block.mData, block.mBytesCount);
            }
            delete mapBlockStorage;
            fclose(mFileDescriptor);
            if (remove(mFileName.c_str()) != 0){
                throw IOError("Can't remove old *.dat file.");
            }
            if (rename(kTempFileName.c_str(), mFileName.c_str()) != 0){
                throw IOError("Can't rename temporary *.dat file to main *.dat file.");
            }
            obtainFileDescriptor();
        }

        void UUIDMapBlockStorage::obtainFileDescriptor() {
            if (isFileExist()) {
                mFileDescriptor = fopen(mFileName.c_str(), kModeUpdate.c_str());
                checkFileDescriptor();
            } else {
                mFileDescriptor = fopen(mFileName.c_str(), kModeCreate.c_str());
                checkFileDescriptor();
                allocateFileHeader();
            }
        }

        void UUIDMapBlockStorage::checkFileDescriptor() {
            if (mFileDescriptor == NULL) {
                throw IOError("Unable to obtain file descriptor");
            }
            mPOSIXFileDescriptor = fileno(mFileDescriptor);
        }

        void UUIDMapBlockStorage::allocateFileHeader() {
            byte *buffer = (byte *) malloc(kFileHeaderSize);
            memset(buffer, 0, kFileHeaderSize);
            fseek(mFileDescriptor, 0, SEEK_SET);
            fwrite(buffer, 1, kFileHeaderSize, mFileDescriptor);
            syncData();
        }

        void UUIDMapBlockStorage::readFileHeader() {
            checkFileDescriptor();
            byte *headerBuffer = (byte *)malloc(kFileHeaderMapIndexOffset);
            memset(headerBuffer, 0, kFileHeaderMapIndexOffset);
            fseek(mFileDescriptor, 0, SEEK_SET);
            fread(headerBuffer, 1, kFileHeaderMapIndexOffset, mFileDescriptor);
            mMapIndexOffset = *((uint32_t *) headerBuffer);
            free(headerBuffer);
            headerBuffer = (byte *)malloc(kFileHeaderMapRecordsCount);
            memset(headerBuffer, 0, kFileHeaderMapRecordsCount);
            fread(headerBuffer, 1, kFileHeaderMapRecordsCount, mFileDescriptor);
            mMapIndexRecordsCount = *((uint64_t *) headerBuffer);
            free(headerBuffer);
        }

        const long UUIDMapBlockStorage::writeData(const byte *block, const size_t blockBytesCount) {
            fseek(mFileDescriptor, 0, SEEK_END);
            long offset = ftell(mFileDescriptor);
            fwrite(block, 1, blockBytesCount, mFileDescriptor);
            syncData();
            return offset;
        }

        const pair<uint32_t, uint64_t> UUIDMapBlockStorage::writeIndexRecordsInMemory(const NodeUUID &uuid, const long &offset,
                                                          const size_t &blockBytesCount) {
            fseek(mFileDescriptor, 0, SEEK_END);
            long offsetToIndexRecords = ftell(mFileDescriptor);
            mIndexBlock.insert(pair<NodeUUID, pair<uint32_t, uint64_t>>(uuid, make_pair((uint32_t) offset, (uint64_t) blockBytesCount)));
            size_t recordsCount = mIndexBlock.size();
            writeIndexBlock();
            return make_pair((uint32_t) offsetToIndexRecords, (uint64_t) recordsCount);
        }

        void UUIDMapBlockStorage::writeFileHeader() {
            fseek(mFileDescriptor, 0, SEEK_SET);
            fwrite(&mMapIndexOffset, 1, kFileHeaderMapIndexOffset, mFileDescriptor);
            fwrite(&mMapIndexRecordsCount, 1, kFileHeaderMapRecordsCount, mFileDescriptor);
            syncData();
        }

        void UUIDMapBlockStorage::writeIndexBlock() {
            map<NodeUUID, pair<uint32_t, uint64_t>>::iterator looper;
            for (looper = mIndexBlock.begin(); looper != mIndexBlock.end(); ++looper){
                fwrite(looper->first.data, 1, kIndexBlockUUIDSize, mFileDescriptor);
                fwrite(&looper->second.first, 1, kIndexBlockOffsetSize, mFileDescriptor);
                fwrite(&looper->second.second, 1, kIndexBlockDataSize, mFileDescriptor);
            }
            syncData();
        }

        void UUIDMapBlockStorage::syncData() {
            fdatasync(mPOSIXFileDescriptor);
        }

        const bool UUIDMapBlockStorage::isFileExist() {
            return fs::exists(fs::path(mFileName.c_str()));
        }

        const bool UUIDMapBlockStorage::isUUIDTheIndex(const NodeUUID &uuid) {
            return mIndexBlock.count(uuid) > 0;
        }


    }
}

