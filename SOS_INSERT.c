/* * * * * * * * * HT_InsertEntry * * * * * * * * */
HT_ErrorCode HT_InsertEntry(int indexDesc, Record record) {
  // Φορτώνουμε το block με τα μεταδεδομένα του αρχείου στην buffer.
  BF_Block* infoBlock;
  BF_Block_Init(&infoBlock);
  
  printf("HT_InsertEntry: openFiles[%d].fd: %d\n\n", indexDesc, openFiles[indexDesc].fd);
  CALL_BF(BF_GetBlock(openFiles[indexDesc].fd, 0, infoBlock));


  HT_Info *info = (HT_Info *)BF_Block_GetData(infoBlock);
  // HT_PrintMetadata(info);


  
  // Υπολογίζουμε την τιμή κατακερματισμού του εκάστοτε record.id.
  int hashValue = hash(record.id, info->globalDepth);
  
  HashTable_resize(&openFiles[indexDesc].hashTable, info);

  BF_Block *bucket;
  BF_Block *newBucket;
  Block_Info blockInfo;
  void * bucketData;

  // i = index του hashTable
  // hashTable[i] == bucket_id
  if(openFiles[indexDesc].hashTable[hashValue] == -1) {
    BF_Block_Init(&bucket);
    CALL_BF(BF_AllocateBlock(info->fileDesc, bucket));

    bucketData = BF_Block_GetData(bucket);

    CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
    blockInfo.block_id = info->num_of_blocks - 1;
    openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
    printf("\nopenFiles[indexDesc].hashTable[hashValue]: %d\n\n", openFiles[indexDesc].hashTable[hashValue]);
    blockInfo.bucket_size = 0;
    blockInfo.local_depth = info->globalDepth;
    blockInfo.hashvalue = hashValue;

    printf("\n\nBlock Info struct:\n");
    printf("blockInfo.block_id: %d\n", blockInfo.block_id);
    printf("blockInfo.bucket_size: %d\n", blockInfo.bucket_size);
    printf("sizeof(blockInfo): %ld\n\n", sizeof(blockInfo));

    
    memcpy(bucketData, &blockInfo, sizeof(Block_Info));
    
  }
  else {
    BF_Block_Init(&bucket);
    CALL_BF(BF_GetBlock(info->fileDesc, openFiles[indexDesc].hashTable[hashValue], bucket));
    bucketData = BF_Block_GetData(bucket);
  }
  Block_Info *ptr = bucketData;
  Record *recordinBlock = (Record *)(bucketData + sizeof(Block_Info) + (ptr->bucket_size * sizeof(Record)));
  
  // Αντιγράφουμε τα δεδομένα του record στα περιεχόμενα του δείκτη *recordinBlock,
  // που δείχνει στην διεύθυνση της επόμενης εγγραφής.
  recordinBlock->id = record.id;

  strncpy(recordinBlock->name, record.name, sizeof(recordinBlock->name));
  recordinBlock->name[sizeof(recordinBlock->name) - 1] = '\0';  // Ensure null-termination


  strncpy(recordinBlock->surname, record.surname, sizeof(recordinBlock->surname));
  recordinBlock->surname[sizeof(recordinBlock->surname) - 1] = '\0';  // Ensure null-termination

  strncpy(recordinBlock->city, record.city, sizeof(recordinBlock->city));
  recordinBlock->city[sizeof(recordinBlock->city) - 1] = '\0';  // Ensure null-termination
  
  if(BF_BLOCK_SIZE > sizeof(Block_Info) + (ptr->bucket_size + 1) * sizeof(Record)) {
    printf("\n\n&bucketData = %p\n", bucketData);
    printf("&recordinBlock = %p\n", recordinBlock);


    printf("Record data: ID = %d, Name = %s, Surname = %s, City = %s\n", record.id, record.name, record.surname, record.city);

    ptr->bucket_size ++;
    info->total_num_of_recs ++;
    printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, recordinBlock->id, recordinBlock->name, recordinBlock->surname, recordinBlock->city);
    printf("Eggrafes mexri stigmhs sto bucket %d: %d\n\n", hashValue, ptr->bucket_size);
    
    CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
    
    BF_Block_SetDirty(bucket);
    BF_Block_SetDirty(infoBlock);
  
  }
  else if(ptr->local_depth == info->globalDepth) {// Σε αυτήν την περίπτωση η εγγραφή δεν χωράει στο block.
    // (1) Διπλασιάζουμε τον πίνακα κατακερματισμού.
    printf("Den xwraei\n");
    
    printf("Eggrafh pros eisagwgh:\nhash: %d, ID: %d, name: %s, surname: %s, city: %s\n\nPalies eggrafes:\n", hashValue, record.id, record.name, record.surname, record.city);

    info->globalDepth ++;
    ptr->local_depth ++;
    HashTable_resize(&openFiles[indexDesc].hashTable, info);


    BF_Block_Init(&newBucket);
    CALL_BF(BF_AllocateBlock(info->fileDesc, newBucket));

    void *newBucketData = BF_Block_GetData(newBucket);

    CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
    blockInfo.block_id = info->num_of_blocks - 1;
    openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
    printf("\nopenFiles[indexDesc].hashTable[hashValue]: %d\n\n", openFiles[indexDesc].hashTable[hashValue]);
    blockInfo.bucket_size = 0;
    blockInfo.local_depth = ptr->local_depth;
    blockInfo.hashvalue = (ptr->hashvalue << 1) + 1;

    printf("\n\nBlock Info struct:\n");
    printf("blockInfo.block_id: %d\n", blockInfo.block_id);
    printf("blockInfo.bucket_size: %d\n", blockInfo.bucket_size);
    printf("sizeof(blockInfo): %ld\n\n", sizeof(blockInfo));

    
    memcpy(newBucket, &blockInfo, sizeof(Block_Info));
    
    if(openFiles[indexDesc].hashTable[blockInfo.hashvalue] == -1)
      openFiles[indexDesc].hashTable[blockInfo.hashvalue] = blockInfo.block_id;
    // for() {

    // }
    printf("%d\n", ptr->block_id);
    Record *records;
    Block_Info *currentBucket;
    void *data;
    int temp = ptr->bucket_size + 1;
    for(int i = 0; i < temp; i++) {
      if(ptr->bucket_size > 0) {
        // Επανατοποθετούμε μία-μία τις εγγραφές στον κατάλληλο κάδο.
        records = (Record *)((char *)bucketData + sizeof(Block_Info) + sizeof(Record) * i);
        printf("\n\nBEFORE hashValue: %d\n", hashValue);
        hashValue = hash(records->id, ptr->local_depth);
        printf("AFTER hashValue: %d\n\n", hashValue);
        ptr->bucket_size --;
      }
      else {
        // Μόλις τελειώσουμε με τις ήδη υπάρχουσες εγγραφές,
        // εξετάζουμε την τιμή κατακερματισμού της δοθείσας.
        hashValue = hash(record.id, blockInfo.local_depth);
        records = &record;
        info->total_num_of_recs ++;
      }

      // Αναλόγως το hash value της εγγραφής οι δείκτες data και currentDirectory δείχνουν
      // στα δεδομένα του bucket και του directory στα οποία θα καταλήξει η εγγραφή.
      if(hashValue == ptr->hashvalue && ptr->bucket_size * sizeof(Record) < BF_BLOCK_SIZE) {
        data = bucketData;
        currentBucket = ptr;
      }
      else if(hashValue == blockInfo.hashvalue && blockInfo.bucket_size * sizeof(Record) < BF_BLOCK_SIZE){ 
        data = newBucketData;
        currentBucket = &blockInfo;
      }
      else {
        BF_Block_SetDirty(bucket);
        BF_Block_SetDirty(newBucket);
        
        CALL_BF(BF_UnpinBlock(newBucket));
        BF_Block_Destroy(&newBucket);

        CALL_BF(BF_UnpinBlock(bucket));
        BF_Block_Destroy(&bucket);

        CALL_BF(BF_UnpinBlock(infoBlock));
        BF_Block_Destroy(&infoBlock);
        printf("skata\n");
        
        CALL_BF(HT_InsertEntry(indexDesc, *records));
      }
      Record *currentRecord = (Record *)((char *)data + sizeof(Record) * currentBucket->bucket_size);
      memcpy(currentRecord, records, sizeof(Record));
      printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, records->id, records->name, records->surname, records->city);
      printf("Eggrafes mexri stigmhs sto bucket me id %d: %d\n\n", currentBucket->block_id, currentBucket->bucket_size+1);

      currentBucket->bucket_size ++;
    }

    BF_Block_SetDirty(bucket);
    BF_Block_SetDirty(newBucket);
    CALL_BF(BF_UnpinBlock(newBucket));
    BF_Block_Destroy(&newBucket);
  }
  CALL_BF(BF_UnpinBlock(bucket));
  BF_Block_Destroy(&bucket);

  CALL_BF(BF_UnpinBlock(infoBlock));
  BF_Block_Destroy(&infoBlock);
  return HT_OK;
}
