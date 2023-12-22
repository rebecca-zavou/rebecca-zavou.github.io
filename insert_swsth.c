
/* * * * * * * * * HT_InsertEntry * * * * * * * * */
HT_ErrorCode HT_InsertEntry(int indexDesc, Record record) {
  // Φορτώνουμε το block με τα μεταδεδομένα του αρχείου στην buffer.
  BF_Block* infoBlock;
  BF_Block_Init(&infoBlock);
  CALL_BF(BF_GetBlock(openFiles[indexDesc].info->fileDesc, 0, infoBlock));
  HT_Info* info = (HT_Info *) BF_Block_GetData(infoBlock);
  
  // Φορτώνουμε το block με τα μεταδεδομένα του πίνακα κατακερματισμού στην buffer.
  BF_Block* hashTableBlock;
  BF_Block_Init(&hashTableBlock);
  CALL_BF(BF_GetBlock(info->fileDesc, 1, hashTableBlock));
  HashTable* hashTable = (HashTable *) BF_Block_GetData(hashTableBlock);
  
  // Υπολογίζουμε την τιμή κατακερματισμού του εκάστοτε record.id.
  int hashValue = hash(record.id, hashTable->global_depth);
  printf("hashValue: %d\n", hashValue);
  

  void* directoryData;
  void* bucketData;

  BF_Block* directoryBlock;
  Directory* directory;
  // Στην 1η κλήση της συνάρτησης δημιουργούμε τον αναγκαίο αριθμό από directories.
  for(int i = 0; hashTable->num_of_directories < power(hashTable->global_depth); i++) {
    if(i == 0)
      HashTable_resize(hashTable);

    // Αρχικοποιούμε και δεσμεύουμε ένα το μπλοκ.
    BF_Block_Init(&directoryBlock);
    CALL_BF(BF_AllocateBlock(info->fileDesc, directoryBlock));

    // Ο δείκτης δείχνει στα δεδομένα του μπλοκ που μόλις δεσμεύσαμε.
    directory = (Directory *)BF_Block_GetData(directoryBlock);

    // Αποθηκεύουμε τον συνολικό αριθμό μπλοκ στην δομή. 
    BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks);

    // Αρχικοποιήση
    directory->directory_id = info->num_of_blocks-1;
    hashTable->directory_ids[i] = directory->directory_id;
    directory->bucket_id = -1;
    directory->bucket_size = 0;
    directory->local_depth = hashTable->global_depth;
    directory->hash_value = i;
    directory->buddies = 0;

    // Δεν δεσμεύουμε bucket στο directory, αφού μπορεί να παραμείνει άδειος.
    directory->bucket_num = 0;
    directory->bucket = NULL;

    hashTable->num_of_directories ++;

    // Κάνουμε το μπλοκ βρώμικο, το ξεκαρφιτσώνουμε και το καταστρέφουμε.
    BF_Block_SetDirty(directoryBlock);
    CALL_BF(BF_UnpinBlock(directoryBlock));
    BF_Block_Destroy(&directoryBlock);
    
    BF_Block_SetDirty(hashTableBlock);
   }

  // Φέρνουμε ένα-ένα τα directories στην μνήμη έως ότου να βρούμε αυτό με το σωστό hash value.
  int directory_id = -1;
  for(int i = 0; i < hashTable->num_of_directories; i++) {
    // Κάθε φορά αρχικοποιούμε το block μέσα στον βρόχο. (*)
    BF_Block_Init(&directoryBlock);
  
    // int num;
    BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks);
    // printf("i = %d, block_num: %d\n", i, num);
    printf("blocks into the file: %d, block pou kalw: %d\n", info->num_of_blocks, hashTable->directory_ids[i]);
    CALL_BF(BF_GetBlock(info->fileDesc, hashTable->directory_ids[i], directoryBlock));
    directory = (Directory *)BF_Block_GetData(directoryBlock);
    printf("id %d: directory->hash_value = %d\n\n", directory->directory_id, directory->hash_value);
    // Ελέγχουμε αν το hash value της δοθείσας εγγραφής ταυτίζεται με το hash value κάποιου
    // ήδη υπάρχοντος directory. Αν ναι, τότε κρατάμε το directory_id του σε μια προσωρινή μεταβλητή.
    if(directory->hash_value == hashValue) {
      directory_id = directory->directory_id;
      break;
    }
    else {
      // (*) Και κάθε φορά το ξεκαρφιτσώνουμε και το καταστρέφουμε.
      CALL_BF(BF_UnpinBlock(directoryBlock));
      BF_Block_Destroy(&directoryBlock);
      }
  }

  BF_Block *bucket;
  printf("directory_id: %d\n", directory_id);

  if(directory_id != -1) {
    BF_Block_Init(&directory->bucket);
    
    if(directory->bucket_num == 0 && directory->buddies == 0) {
      BF_AllocateBlock(info->fileDesc, directory->bucket);
      info->num_of_blocks ++;
      info->num_of_buckets ++;
      directory->bucket_num = 1;
      directory->bucket_id = info->num_of_blocks - 1;

    }
    else {
      BF_GetBlock(info->fileDesc, directory->bucket_id, directory->bucket);
    }

    BF_Block *bucketData = (BF_Block *)BF_Block_GetData(directory->bucket);

    Record *recordinBlock = (Record *)((char *)bucketData + sizeof(Record) * directory->bucket_size);
    
    // Ελέγχουμε αν χωράει η εγγραφή στο block
    if (sizeof(Record) * (directory->bucket_size + 1) < BF_BLOCK_SIZE) {
      // Copy the record into the block
      memcpy(recordinBlock, &record, sizeof(Record));
      
      printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, recordinBlock->id, recordinBlock->name, recordinBlock->surname, recordinBlock->city);
      directory->bucket_size++;
      printf("Eggrafes mexri stigmhs sto bucket %d: %d\n\n", directory->hash_value, directory->bucket_size);

      info->total_num_of_recs ++;


      // Κάνουμε τα μπλοκς βρώμικα, τα ξεκαρφιτσώνουμε και τα καταστρέφουμε.

      BF_Block_SetDirty(directory->bucket);
      CALL_BF(BF_UnpinBlock(directory->bucket));
      BF_Block_Destroy(&directory->bucket);

      BF_Block_SetDirty(directoryBlock);
      CALL_BF(BF_UnpinBlock(directoryBlock));
      BF_Block_Destroy(&directoryBlock);


      CALL_BF(BF_UnpinBlock(hashTableBlock));
      BF_Block_Destroy(&hashTableBlock);

      BF_Block_SetDirty(infoBlock);
      CALL_BF(BF_UnpinBlock(infoBlock));
      BF_Block_Destroy(&infoBlock);

      return HT_OK;
    }
    else if(directory->local_depth == hashTable->global_depth){ // Σε αυτήν την περίπτωση η εγγραφή δεν χωράει στο block.
      // (1) Διπλασιάζουμε τον πίνακα κατακερματισμού.
      printf("Den xwraei\n");
      
      printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, record.id, record.name, record.surname, record.city);
      
      // hashTable->num_of_directories *= 2;
      
      // Δημιουργούμε ένα νέο directory για τις εγγραφές που δεν χωρούσαν.
      BF_Block *new_directory_block;
      BF_Block_Init(&new_directory_block);
      CALL_BF(BF_AllocateBlock(info->fileDesc, new_directory_block));
      Directory *newDirectory = (Directory *)BF_Block_GetData(new_directory_block);

      hashTable->num_of_directories ++;

      // (3) Αύξηση του global depth κατά 1.
      hashTable->global_depth ++;

      info->num_of_blocks ++;
      info->num_of_buckets ++;

      newDirectory->directory_id = info->num_of_blocks-1;
      HashTable_resize(hashTable);

      for(int i = 0; i < hashTable->num_of_directories; i++)
        if(hashTable->directory_ids[i] == -1) {
          hashTable->directory_ids[i] = newDirectory->directory_id;
          printf("OLA KALA! hashTable->directory_ids[i]: %d, newDirectory->directory_id: %d\n", hashTable->directory_ids[i], newDirectory->directory_id);
          break;
        }

      printf("newDir id: %d\n", newDirectory->directory_id);
      newDirectory->bucket_num = 0;
      newDirectory->bucket_size = 0;
      newDirectory->buddies = 0;
      
      printf("(old dir) old hash value: %d\n", directory->hash_value);
      // Η καινούρια τιμή κατακερματισμού για τις υπάρχουσες θέσεις στον πίνακα προκύπτει
      // από την δεξιά ολίσθηση της παλιάς τιμής κατά 1.
      directory->hash_value <<= 1;
      printf("(old dir) new hash value: %d\n", directory->hash_value);
      
      newDirectory->hash_value = directory->hash_value + 1;
      printf("(new dir)(id: %d) new hash value: %d\n", newDirectory->directory_id, newDirectory->hash_value);
      
      // Ενημέρωση του ολικού βάθους.
      newDirectory->local_depth = hashTable->global_depth;
      directory->local_depth = hashTable->global_depth;


      // (4) Διάσπαση του κάδου σε 2.
      BF_Block_Init(&newDirectory->bucket);
      CALL_BF(BF_AllocateBlock(info->fileDesc, newDirectory->bucket));


      newDirectory->bucket_id = newDirectory->directory_id + 1;
      info->num_of_blocks ++;

      // (6) Επανυπολογισμός της τιμής κατακερματισού όλων των εγγραφών στον ήδη υπάρχων κάδο.
      void* newBucketdata = BF_Block_GetData(newDirectory->bucket);
      void* data;
      Directory *currentDirectory;

      Record *records;
      int temp = directory->bucket_num + 1;
      int oldDirBucketSize = directory->bucket_size;
      directory->bucket_size = 0;
      for(int i = 0; i < temp; i++) {
        if(oldDirBucketSize > 0) {
          // Επανατοποθετούμε μία-μία τις εγγραφές στον κατάλληλο κάδο.
          records = (Record *)((char *)bucketData + sizeof(Record) * i);
          printf("directory->local_depth: %d\n", directory->local_depth);
          hashValue = hash(records->id, directory->local_depth);
          oldDirBucketSize --;
        }
        else {
          // Μόλις τελειώσουμε με τις ήδη υπάρχουσες εγγραφές,
          // εξετάζουμε την τιμή κατακερματισμού της δοθείσας.
          hashValue = hash(record.id, directory->local_depth);
          records = &record;
          info->total_num_of_recs ++;
        }

        // Αναλόγως το hash value της εγγραφής οι δείκτες data και currentDirectory δείχνουν
        // στα δεδομένα του bucket και του directory στα οποία θα καταλήξει η εγγραφή.
        if(hashValue == directory->hash_value && directory->bucket_size * sizeof(Record) < BF_BLOCK_SIZE) {
          data = bucketData;
          currentDirectory = directory;
        }
        else {
          printf("skata\n");
          // CALL_BF(HT_InsertEntry(indexDesc, *records));
        }
        if(hashValue == newDirectory->hash_value && newDirectory->bucket_size * sizeof(Record) < BF_BLOCK_SIZE){ 
          data = newBucketdata;
          currentDirectory = newDirectory;
        }
        Record *currentRecord = (Record *)((char *)data + sizeof(Record) * currentDirectory->bucket_size);
        memcpy(currentRecord, records, sizeof(Record));
        printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, records->id, records->name, records->surname, records->city);
        printf("Eggrafes mexri stigmhs sto bucket me id %d: %d\n\n", currentDirectory->directory_id, currentDirectory->bucket_size+1);

        currentDirectory->bucket_size ++;
      }
      
      BF_Block_SetDirty(newDirectory->bucket);
      CALL_BF(BF_UnpinBlock(newDirectory->bucket));
      BF_Block_Destroy(&newDirectory->bucket);

      BF_Block_SetDirty(new_directory_block);
      CALL_BF(BF_UnpinBlock(new_directory_block));
      BF_Block_Destroy(&new_directory_block);

      // HashTable_resize(hashTable);
      // Δημιουργούμε τις υπόλοιπες θέσεις (directories) τον πίνακα κατακερματισμού.
      for(; hashTable->num_of_directories < power(hashTable->global_depth); ) {
        BF_Block_Init(&new_directory_block);
        CALL_BF(BF_AllocateBlock(info->fileDesc, new_directory_block));
        newDirectory = (Directory *)BF_Block_GetData(new_directory_block);

        info->num_of_blocks ++;
        info->num_of_buckets ++;
        newDirectory->directory_id = info->num_of_blocks-1;

        for(int i = 0; i < hashTable->num_of_directories; i++)
          if(hashTable->directory_ids[i] == -1) {
            hashTable->directory_ids[i] = newDirectory->directory_id;
            break;
          }

        
      printf("newDir id: %d, info->num_of_blocks-1: %d\n", newDirectory->directory_id, info->num_of_blocks-1);
        newDirectory->bucket_num = 0;
        newDirectory->bucket_size = 0;
        newDirectory->local_depth = hashTable->global_depth;

        for(int k = 0; k < hashTable->num_of_directories / 2; k ++) {
          BF_Block* oldDirectoryBlock;
          BF_Block_Init(&oldDirectoryBlock);
          CALL_BF(BF_GetBlock(info->fileDesc, directory_id + k, oldDirectoryBlock));
          Directory *oldDirectory = (Directory *)BF_Block_GetData(oldDirectoryBlock);


          if(oldDirectory->bucket_num == 1 && oldDirectory->buddies == 0 && oldDirectory->local_depth < hashTable->global_depth) {
            oldDirectory->hash_value <<= 1;
            newDirectory->hash_value = oldDirectory->hash_value + 1;
            newDirectory->buddies = 1;
            newDirectory->bucket_id = oldDirectory->bucket_id;
            newDirectory->bucket = oldDirectory->bucket;

            
            printf("Edw: ");
            printf("%d bucket's directory_id: %d and bucket_id: %d, newDirectory->buddies: %d\n", hashTable->num_of_directories, newDirectory->directory_id, newDirectory->bucket_id, newDirectory->buddies);


            BF_Block_SetDirty(oldDirectoryBlock);
            CALL_BF(BF_UnpinBlock(oldDirectoryBlock));
            BF_Block_Destroy(&oldDirectoryBlock);
            break;
          }

            hashTable->num_of_directories ++;
        }
        
        BF_Block_SetDirty(new_directory_block);
        CALL_BF(BF_UnpinBlock(new_directory_block));
        BF_Block_Destroy(&new_directory_block);
      }

      BF_Block_SetDirty(directoryBlock);
      CALL_BF(BF_UnpinBlock(directoryBlock));
      BF_Block_Destroy(&directoryBlock);
      ////
      ////
      ////
      ////
      ////
      BF_Block_SetDirty(hashTableBlock);
      CALL_BF(BF_UnpinBlock(hashTableBlock));
      BF_Block_Destroy(&hashTableBlock);

      BF_Block_SetDirty(infoBlock);
      CALL_BF(BF_UnpinBlock(infoBlock));
      BF_Block_Destroy(&infoBlock);
    }
  }
  else {
    CALL_BF(BF_UnpinBlock(directoryBlock));
    BF_Block_Destroy(&directoryBlock);

    CALL_BF(BF_UnpinBlock(hashTableBlock));
    BF_Block_Destroy(&hashTableBlock);

    CALL_BF(BF_UnpinBlock(infoBlock));
    BF_Block_Destroy(&infoBlock);
    return HT_ERROR;
  }

  
  return HT_OK;
}
