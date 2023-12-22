#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hash_file.h"
#define MAX_OPEN_FILES 20


#define TABLE_SIZE 3
#define HT_OK 0
#define HT_ERROR -1

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return HT_ERROR;        \
  }                         \
}
static struct {
  int file_desc;
  int *hashTable;
} openFiles[MAX_OPEN_FILES];

HT_ErrorCode HT_Init() {
  BF_Init(LRU);
  // Αρχικοποιύμε κάθε θέση του πίνακα.
  for (int i = 0; i < MAX_OPEN_FILES; ++i) {
    openFiles[i].file_desc = -1;
    
    if (i == 0) {
      // Δεσμεύουμε δυναμικά μνήμη ανάλογα με το TABLE_SIZE.
      openFiles[i].hashTable = (int *)malloc(TABLE_SIZE * sizeof(int));
      if (openFiles[i].hashTable == NULL) {
        return HT_ERROR;
      }
      
      for (int j = 0; j < TABLE_SIZE; ++j) {
        openFiles[i].hashTable[j] = -1;
        printf("openFiles[%d].hashTable[%d] = %d\n", i, j, openFiles[i].hashTable[j]);
      }
    }
    else {
        openFiles[i].hashTable = NULL;
      }
  }
  return HT_OK;
}

HT_ErrorCode HT_CreateIndex(const char *filename, int depth) {
  // Ελέγχουμε αν το αρχείο υπάρχει ήδη.
  BF_ErrorCode code = BF_CreateFile(filename);
  if (code == BF_FILE_ALREADY_EXISTS) {
    printf("ERROR. The file you are trying to create already exists!\n\n");
    return HT_ERROR;
  }

  // Ανοίγουμε το αρχείο και επιστρέφουμε το file descriptor του στην μεταβλητή file_desc.
  int file_desc;
  CALL_BF(BF_OpenFile(filename, &file_desc)); // Open the created file

  // Ορίζουμε, αρχικοποιούμε και δεσμεύουμε ένα block, για τα μεταδεδομένα του αρχείου.
  BF_Block *infoBlock;
  BF_Block_Init(&infoBlock);
  CALL_BF(BF_AllocateBlock(file_desc, infoBlock));
  
  HT_Info info;

  // Ορίζουμε έναν δείκτη που δείχνει στα δεδομένα του block των μεταδεδομένων.
  void *infoData = BF_Block_GetData(infoBlock);
  memset(infoData, 0, sizeof(BF_Block *));
        
  strncpy(info.fileType, "Hash File", sizeof(info.fileType));
  info.fileType[sizeof(info.fileType) - 1] = '\0';
  
  strncpy(info.fileName, filename, sizeof(info.fileName));
  info.fileName[sizeof(info.fileName) - 1] = '\0';
  
  strncpy(info.hash_field, "id", sizeof(info.hash_field));
  info.hash_field[sizeof(info.hash_field) - 1] = '\0';

  info.fileDesc = file_desc;
  info.total_num_of_recs = 0;
  info.num_of_blocks = 1;
  info.globalDepth = depth;


  memcpy(infoData, &info, sizeof(HT_Info));
  HT_PrintMetadata(infoData);

  // Κάνουμε το block βρώμικο, το ξεκαρφιτσώνουμε και το καταστρέφουμε.
  BF_Block_SetDirty(infoBlock);
  CALL_BF(BF_UnpinBlock(infoBlock));
  BF_Block_Destroy(&infoBlock);

  CALL_BF(BF_CloseFile(file_desc));

  return HT_OK;
}


HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc){
  // Finding an available slot in the open_files array
  int i;
  for (i = 0; i < MAX_OPEN_FILES; ++i) {
    if (openFiles[i].file_desc == -1) {
      break;  // Found an available slot
    }
  }

  if (i == MAX_OPEN_FILES) {
    printf("MAX OPEN FILES, NO MORE SLOTS AVAILABLE\n");
    return HT_ERROR; // No available slots
  }

  // Opening the file and storing the file descriptor in the open_files array
  BF_ErrorCode bf_code = BF_OpenFile(fileName, &openFiles[i].file_desc);
  printf("openFiles[%d].file_desc: %d\n", i, openFiles[i].file_desc);
  if (bf_code != BF_OK) {
    BF_PrintError(bf_code);
    return HT_ERROR;  // Opening file failed
  }

  *indexDesc = i;  // Storing the index of the open file

  return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc) {
  
  // Ελέγχουμε αν ο δεικτής που μας δόθηκε αντιστοιχεί σε κάποια θέση θέση του πίνακα,
  // κι αν ναι, ελέγχουμε αν αυτή η θέση περιέχει κάποιο αρχείο.
  if (indexDesc < 0 || indexDesc >= MAX_OPEN_FILES || openFiles[indexDesc].file_desc == -1) {
    return HT_ERROR;
  }

  // Καλούμε την BF_CloseFile() για να κλείσουμε το αρχείο.
  BF_ErrorCode code = BF_CloseFile(openFiles[indexDesc].file_desc);
  
  // Αν η BF_CloseFile() επιστρέψει κωδικό λάθους, τότε τερματίζουμε την συνάρτηση με κωδικό λάθους.
  if (code != BF_OK) {
    return HT_ERROR;
  }

  if (openFiles[indexDesc].hashTable != NULL) {
    // Ελευθερώνουμε την μνήμη που δεσμεύσαμε δυναμικά.
    free(openFiles[indexDesc].hashTable);
    openFiles[indexDesc].hashTable = NULL;
  }

  // "Αδειάζουμε" την θέση του πίνακα που αντιστοιχούσε στο αρχείο που μόλις κλείσαμε.
  openFiles[indexDesc].file_desc = -1;
  openFiles[indexDesc].hashTable = NULL;

  return HT_OK;
}



/* * * * * * * * * HT_InsertEntry * * * * * * * * */
HT_ErrorCode HT_InsertEntry(int indexDesc, Record record) {
  printf("\n\nI N S E R T \n\n");
  // Φορτώνουμε το block με τα μεταδεδομένα του αρχείου στην buffer.
  BF_Block* infoBlock;
  BF_Block_Init(&infoBlock);
  CALL_BF(BF_GetBlock(openFiles[indexDesc].file_desc, 0, infoBlock));

  // Ο δείκτης *ινφο δείχνει στα μεταδεδομένα του 1ου block.
  HT_Info *info = (HT_Info *)BF_Block_GetData(infoBlock);
  // HT_PrintMetadata(info);
  

  // Εκτύπωση του πίνακα κατακερατισμού.
  Print_Hash_Table(openFiles[indexDesc].hashTable, info);
  
  // Υπολογίζουμε την τιμή κατακερματισμού του εκάστοτε record.id.
  int hashValue = hash(record.id, info->globalDepth);
  printf("hash value: %d\n\n", hashValue);
  
  

  BF_Block *bucket;           // Το bucket στο οποίο αντστοιχεί το record, με βάση το αρχικό hashvalue του, άσχετα με το αν χωράει σε αυτό.
  BF_Block *newBucket;        // Σε περίπτωση που χρειαστεί να δημιουργήσυμε καινούριο bucket.
  BF_Block *buddyOld;
  BF_Block *buddyNew;
  
  Block_Info blockInfo;

  void *bucketData;
  Block_Info *buddyOldData;
  int oldBucket_id = -1;

  // 1η κλήση της συνάρτησης,
  // Δημιουργία 1ου bucket.
  if(info->num_of_blocks == 1 && openFiles[indexDesc].hashTable[hashValue] == -1) {
    printf("info->num_of_blocks: %d, openFiles[0].hashTable[%d]: %d\n", info->num_of_blocks, hashValue, openFiles[indexDesc].hashTable[hashValue]);
    printf("CREATING THE VERY FIRST BLOCK\n");
    
    BF_Block_Init(&bucket);
    CALL_BF(BF_AllocateBlock(info->fileDesc, bucket));

    bucketData = BF_Block_GetData(bucket);

    CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
    blockInfo.block_id = info->num_of_blocks - 1;
    openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
    oldBucket_id = blockInfo.block_id;
    printf("\nopenFiles[0].hashTable[%d]: %d\n\n", hashValue, openFiles[indexDesc].hashTable[hashValue]);
    blockInfo.bucket_size = 0;
    blockInfo.local_depth = info->globalDepth;
    blockInfo.buddiesBoolean = 0;

    printf("\n\nBlock Info struct:\n");
    printf("blockInfo.block_id: %d\n", blockInfo.block_id);
    printf("blockInfo.bucket_size: %d\n", blockInfo.bucket_size);
    printf("sizeof(blockInfo): %ld\n\n", sizeof(blockInfo));

  
    // memset(bucketData, 0, sizeof(BF_Block *));
    memset(bucketData, 0, sizeof(Record));
    memcpy(bucketData, &blockInfo, sizeof(Block_Info));
  }
  // Υπάρχει θέση με αυτό το hash value στον πίνακα κατακερματισμού, αλλά δεν της αντιστοιχεί κάποιο block ακόμα.
  else {
    
    if(openFiles[indexDesc].hashTable[hashValue] == -1) {
      printf("openFiles[0].hashTable[%d]: %d\n", hashValue, openFiles[indexDesc].hashTable[hashValue]);
      
      // if(info->num_of_blocks-1 < power(info->globalDepth) && info->num_of_blocks != 2) {
      //   for(int old = 0; old < info->num_of_blocks-1; old ++) {
      //     for(int new = old+1; new < info->num_of_blocks-1; new ++) {
      //       if(openFiles[indexDesc].hashTable[new] == -1) {
      //         BF_Block_Init(&buddyOld);
      //         CALL_BF(BF_GetBlock(info->fileDesc, openFiles[indexDesc].hashTable[old], buddyOld));
      //         buddyOldData = (Block_Info *)BF_Block_GetData(buddyOld);
      //         if(buddyOldData->local_depth < info->globalDepth && (old<<1)+1 == new) {
      //           openFiles[indexDesc].hashTable[new] = buddyOldData->block_id;
      //           printf("BUDDIES: block: %d, block: %d\n", old, new);
      //         }
      //         CALL_BF(BF_UnpinBlock(buddyOld));
      //         BF_Block_Destroy(&buddyOld);
      //       }
      //     }
      //   }
      //   BF_Block_Init(&bucket);
      //   CALL_BF(BF_GetBlock(info->fileDesc, openFiles[indexDesc].hashTable[hashValue], bucket));
      //   bucketData = BF_Block_GetData(bucket);
      // }
      // Eίναι η 1η εγγραφή με αυτό το hash value,
      // φτιάχνουμε καινούριο bucket.
       {
        printf("\n- - - FTIAXNW BUCKET GIA \"PRWTEUON\" BLOCK: %d- - -\n", hashValue);
        printf("info->num_of_blocks: %d, openFiles[0].hashTable[%d]: %d\n", info->num_of_blocks, hashValue, openFiles[indexDesc].hashTable[hashValue]);
    
        BF_Block_Init(&bucket);
        CALL_BF(BF_AllocateBlock(info->fileDesc, bucket));

        bucketData = BF_Block_GetData(bucket);

        CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
        blockInfo.block_id = info->num_of_blocks - 1;
        openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
        printf("\nopenFiles[0].hashTable[%d]: %d\n\n", hashValue, openFiles[indexDesc].hashTable[hashValue]);
        blockInfo.bucket_size = 0;
        blockInfo.local_depth = 1;
        blockInfo.buddiesBoolean = 0;

        printf("\n\nBlock Info struct:\n");
        printf("blockInfo.block_id: %d\n", blockInfo.block_id);
        printf("blockInfo.bucket_size: %d\n", blockInfo.bucket_size);
        printf("blockInfo.local_depth: %d\n", blockInfo.local_depth);
        printf("sizeof(blockInfo): %ld\n\n", sizeof(blockInfo));

        // memset(bucketData, 0, sizeof(BF_Block *));
        memset(bucketData, 0, sizeof(Record));
        memcpy(bucketData, &blockInfo, sizeof(Block_Info));
        
        // BF_Block_SetDirty(infoBlock);
        // BF_Block_SetDirty(bucket);
      }
    }
    else {
      
      printf("info->num_of_blocks: %d, openFiles[0].hashTable[%d]: %d\n", info->num_of_blocks, hashValue, openFiles[indexDesc].hashTable[hashValue]);
    
      BF_Block_Init(&bucket);
      CALL_BF(BF_GetBlock(info->fileDesc, openFiles[indexDesc].hashTable[hashValue], bucket));
      bucketData = BF_Block_GetData(bucket);
    }
  }
  Block_Info *ptr = (Block_Info *)bucketData;

  
  // Η εγγραφή χωράει στο bucket που της αντιστοιχεί.
  // BF_BLOCK_SIZE - sizeof(Block_Info) - ptr->bucket_size * sizeof(Record) > sizeof(Record)
  if(ptr->bucket_size < 7) {
    Record* recordinBlock = (Record*)malloc(sizeof(Record));
    recordinBlock = (Record *)(bucketData + ((ptr->bucket_size + 1) * sizeof(Record)));


    
    memset(recordinBlock, 0, sizeof(Record));

    // Αντιγράφουμε τα δεδομένα του record στα περιεχόμενα του δείκτη *recordinBlock,
    // που δείχνει στην διεύθυνση της επόμενης εγγραφής.
    recordinBlock->id = record.id;

    strncpy(recordinBlock->name, record.name, sizeof(recordinBlock->name));
    recordinBlock->name[sizeof(recordinBlock->name) - 1] = '\0';

    strncpy(recordinBlock->surname, record.surname, sizeof(recordinBlock->surname));
    recordinBlock->surname[sizeof(recordinBlock->surname) - 1] = '\0';

    strncpy(recordinBlock->city, record.city, sizeof(recordinBlock->city));
    recordinBlock->city[sizeof(recordinBlock->city) - 1] = '\0';

    
    printf("\n\n&bucketData = %p\n", bucketData);
    printf("&recordinBlock = %p\n", recordinBlock);


    printf("Record data: ID = %d, Name = %s, Surname = %s, City = %s\n", record.id, record.name, record.surname, record.city);

    ptr->bucket_size ++;
    info->total_num_of_recs ++;
    printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, recordinBlock->id, recordinBlock->name, recordinBlock->surname, recordinBlock->city);
    printf("Eggrafes mexri stigmhs sto bucket %d: %d\n\n", hashValue, ptr->bucket_size);
    
    
    BF_Block_SetDirty(bucket);
    BF_Block_SetDirty(infoBlock);

    CALL_BF(BF_UnpinBlock(bucket));
    BF_Block_Destroy(&bucket);

    CALL_BF(BF_UnpinBlock(infoBlock));
    BF_Block_Destroy(&infoBlock);
    return HT_OK;
  }
  else {
    printf("\n\n- - - - - - - - - D E N  X W R A E I - - - - - - - - -\n");
    void *newBucketData;
    int newBucket_id = -1;

    // Τ Ο Π Ι Κ Ο  Β Α Θ Ο Σ  ==  Ο Λ Ι Κ Ο  Β Α Θ Ο Σ
    if(ptr->local_depth == info->globalDepth) {// Σε αυτήν την περίπτωση η εγγραφή δεν χωράει στο block.
      printf("\nL O C A L  D E P T H  ==  G L O B A L  D E P T H\n\n");
      // (1) Διπλασιάζουμε τον πίνακα κατακερματισμού.
      printf("Eggrafh pros eisagwgh:\nhash: %d, ID: %d, name: %s, surname: %s, city: %s\n\nPalies eggrafes:\n", hashValue, record.id, record.name, record.surname, record.city);

      info->globalDepth ++;
      ptr->local_depth ++;
      printf("\n\n\n!!!!!!!!!!!!!!!!!!!! EDW TO KANW RESIZE !!!!!!!!!!!!!!!!!!!!");
      HashTable_resize(&openFiles[indexDesc].hashTable, info);
      Print_Hash_Table(openFiles[indexDesc].hashTable, info);

      BF_Block_Init(&newBucket);
      CALL_BF(BF_AllocateBlock(info->fileDesc, newBucket));

      newBucketData = BF_Block_GetData(newBucket);

      CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
      newBucket_id = blockInfo.block_id;  // might need it in recursion

      int numOfPlacesInTheTable = power(info->globalDepth);
      blockInfo.block_id = info->num_of_blocks - 1;
      for(int l = 0; l < numOfPlacesInTheTable; l ++)
        if(openFiles[indexDesc].hashTable[l] == -1) {
          openFiles[indexDesc].hashTable[l] = blockInfo.block_id;
          // continue;
        }


      
      blockInfo.bucket_size = 0;
      blockInfo.buddiesBoolean = 0;
      blockInfo.local_depth = ptr->local_depth;
      Print_Hash_Table(openFiles[indexDesc].hashTable, info);

      printf("\n\nBlock Info struct:\n");printf("blockInfo.bucket_size: %d\n", blockInfo.bucket_size);
      printf("sizeof(blockInfo): %ld\n\n", sizeof(blockInfo));

      // memset(newBucket, 0, sizeof(Record));
      memcpy(newBucketData, &blockInfo, sizeof(Block_Info));
      // Block_Info *buddyData;
      
      

      printf("%d\n", ptr->block_id);
      Record *records;
      Block_Info *currentBucket;
      void *data;

      int temp = ptr->bucket_size+1;
      int fakeSizeForOldBucket = ptr->bucket_size;
      int *currentSize = 0;
      ptr->bucket_size = 0;
      Record recursionRec;
      oldBucket_id = ptr->block_id;
      newBucket_id = blockInfo.block_id;
      int recursionFlag = 0;
      for(int i = 0; i < temp; i++) {
        int flag = 0;
        if(i < temp - 1) {
          bucketData = BF_Block_GetData(bucket);
          // Επανατοποθετούμε μία-μία τις εγγραφές στον κατάλληλο κάδο.
          records = (Record *)(bucketData + sizeof(Record) * (i+1));
                
          
          printf("\n\n - - - - - %dH EPANALHPSH - - - - -\n- - - EGGRAFH PROS EISAGWGH: - - -\n\nhash: %d, ID: %d, name: %s, surname: %s, city: %s\n", i+1, hashValue, records->id, records->name, records->surname, records->city);
          printf("\n\n&bucketData = %p\n", bucketData);
          printf("&newBucketData = %p\n", newBucketData);
          printf("&records = %p\n", records);

          printf("\nBEFORE hashValue: %d\n", hashValue);
          hashValue = hash(records->id, info->globalDepth);
          printf("AFTER hashValue: %d\n", hashValue);
        }
        else {
          printf("\n\n - - - - - %dH EPANALHPSH - - - - -\n- - - EGGRAFH PROS EISAGWGH: - - -\n\nhash: %d, ID: %d, name: %s, surname: %s, city: %s\n", i+1, hashValue, record.id, record.name, record.surname, record.city);
          // Μόλις τελειώσουμε με τις ήδη υπάρχουσες εγγραφές,
          // εξετάζουμε την τιμή κατακερματισμού της δοθείσας.
          hashValue = hash(record.id, info->globalDepth);
          records = &record;
          info->total_num_of_recs ++;
        }

        printf("xwraei sto palio (%d)? (bucket_size): %d\n", oldBucket_id, fakeSizeForOldBucket);
        printf("xwraei sto neo (%d)? (bucket_size): %d\n", newBucket_id, blockInfo.bucket_size);

        // Αναλόγως το hash value της εγγραφής οι δείκτες data και currentDirectory δείχνουν
        // στα δεδομένα του bucket και του directory στα οποία θα καταλήξει η εγγραφή.
        if(oldBucket_id == openFiles[indexDesc].hashTable[hashValue] && (ptr->bucket_size < 7)) {
          data = bucketData;
          currentBucket = ptr;
          currentSize = &ptr->bucket_size;
          printf("MPHKE STO %d\n", oldBucket_id);
        }
        else if(newBucket_id == openFiles[indexDesc].hashTable[hashValue] && (blockInfo.bucket_size < 7)){ 
          data = newBucketData;
          currentBucket = &blockInfo;
          currentSize = &blockInfo.bucket_size;
          printf("MPHKE STO %d\n", newBucket_id);
        }
        else {
          // ptr->bucket_size = fakeSizeForOldBucket;
          // recursionRec.id = records->id;
            
          // strncpy(recursionRec.name, records->name, sizeof(recursionRec.name));
          // recursionRec.name[sizeof(recursionRec.name) - 1] = '\0'; 
          
          // strncpy(recursionRec.surname, records->surname, sizeof(recursionRec.surname));
          // recursionRec.name[sizeof(recursionRec.surname) - 1] = '\0';

          // strncpy(recursionRec.city, records->city, sizeof(recursionRec.city));
          // recursionRec.city[sizeof(recursionRec.city) - 1] = '\0';

          printf("- - - A N A D R O M H - - -\n");

          CALL_BF(HT_InsertEntry(indexDesc, *records));
          flag = 1;
          recursionFlag = 1;
        }
        if(!flag) {
          Record *currentRecord = (Record *)((char *)data + sizeof(Record) * (*currentSize));

          memset(currentRecord, 0, sizeof(BF_Block *));

          recursionRec.id = records->id;
              
          strncpy(recursionRec.name, records->name, sizeof(recursionRec.name));
          recursionRec.name[sizeof(recursionRec.name) - 1] = '\0';
          
          strncpy(recursionRec.surname, records->surname, sizeof(recursionRec.surname));
          recursionRec.name[sizeof(recursionRec.surname) - 1] = '\0';

          strncpy(recursionRec.city, records->city, sizeof(recursionRec.city));
          recursionRec.city[sizeof(recursionRec.city) - 1] = '\0';

          // memcpy(currentRecord, records, sizeof(Record));
          printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, records->id, records->name, records->surname, records->city);
          *currentSize += 1;
          printf("Eggrafes mexri stigmhs sto bucket me id %d: %d\n\n", currentBucket->block_id, (*currentSize));
        }
      }
      if(!recursionFlag) {
        BF_Block_SetDirty(newBucket);
        CALL_BF(BF_UnpinBlock(newBucket));
        BF_Block_Destroy(&newBucket);

    
        BF_Block_SetDirty(bucket);
        CALL_BF(BF_UnpinBlock(bucket));
        BF_Block_Destroy(&bucket);

        BF_Block_SetDirty(infoBlock);
        CALL_BF(BF_UnpinBlock(infoBlock));
        BF_Block_Destroy(&infoBlock);
      }
      // ptr->bucket_size = fakeSizeForOldBucket;
      return HT_OK;
    }

    // Τ Ο Π Ι Κ Ο  Β Α Θ Ο Σ  <  Ο Λ Ι Κ Ο  Β Α Θ Ο Σ
    else if(ptr->local_depth < info->globalDepth) {
      printf("L O C A L  D E P T H  <  G L O B A L  D E P T H\n\n");
      Print_Hash_Table(openFiles[indexDesc].hashTable, info);

      BF_Block_Init(&newBucket);
      CALL_BF(BF_AllocateBlock(info->fileDesc, newBucket));

      newBucketData = BF_Block_GetData(newBucket);

      CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
      newBucket_id = blockInfo.block_id;  // might need it in recursion

      openFiles[indexDesc].hashTable[hashValue] = newBucket_id;


      ptr->local_depth ++;
      blockInfo.bucket_size = 0;
      blockInfo.buddiesBoolean = 0;
      blockInfo.local_depth = ptr->local_depth;
      Print_Hash_Table(openFiles[indexDesc].hashTable, info);

      printf("\n\nBlock Info struct:\n");printf("blockInfo.bucket_size: %d\n", blockInfo.bucket_size);
      printf("sizeof(blockInfo): %ld\n\n", sizeof(blockInfo));

      // memset(newBucket, 0, sizeof(Record));
      memcpy(newBucketData, &blockInfo, sizeof(Block_Info));
      // Block_Info *buddyData;
      
    
      Record* recordinBlock = (Record*)malloc(sizeof(Record));
      recordinBlock = (Record *)(newBucketData + ((ptr->bucket_size + 1) * sizeof(Record)));


      memset(recordinBlock, 0, sizeof(Record));

      // Αντιγράφουμε τα δεδομένα του record στα περιεχόμενα του δείκτη *recordinBlock,
      // που δείχνει στην διεύθυνση της επόμενης εγγραφής.
      recordinBlock->id = record.id;

      strncpy(recordinBlock->name, record.name, sizeof(recordinBlock->name));
      recordinBlock->name[sizeof(recordinBlock->name) - 1] = '\0';  // Ensure null-termination

      strncpy(recordinBlock->surname, record.surname, sizeof(recordinBlock->surname));
      recordinBlock->surname[sizeof(recordinBlock->surname) - 1] = '\0';  // Ensure null-termination

      strncpy(recordinBlock->city, record.city, sizeof(recordinBlock->city));
      recordinBlock->city[sizeof(recordinBlock->city) - 1] = '\0';  // Ensure null-termination

      
      printf("\n\n&bucketData = %p\n", bucketData);
      printf("&recordinBlock = %p\n", recordinBlock);


      printf("Record data: ID = %d, Name = %s, Surname = %s, City = %s\n", record.id, record.name, record.surname, record.city);

      ptr->bucket_size ++;
      info->total_num_of_recs ++;
      printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, recordinBlock->id, recordinBlock->name, recordinBlock->surname, recordinBlock->city);
      printf("Eggrafes mexri stigmhs sto bucket %d: %d\n\n", hashValue, ptr->bucket_size);
      
      
      BF_Block_SetDirty(bucket);
      BF_Block_SetDirty(infoBlock);

      CALL_BF(BF_UnpinBlock(bucket));
      BF_Block_Destroy(&bucket);

      CALL_BF(BF_UnpinBlock(infoBlock));
      BF_Block_Destroy(&infoBlock);
      return HT_OK;
    }


      
      return HT_OK;
  }
  return HT_ERROR;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {

    return HT_OK;

}



// Συναρτήσεις που δημιουργήσαμε:


// Hash function
unsigned int hash(unsigned int key, unsigned int depth) {
  unsigned int hashValue = key * 999999937;
  hashValue = hashValue >> (32 - depth);
  return hashValue;
}

int max_bits(int maxNumber) {
  int maxBits = 0;
  while (maxNumber > 0) {
      maxNumber >>= 1;
      maxBits++;
  }
  return maxBits;
}
void HT_PrintMetadata(void *data) {
  HT_Info *info_ptr = (HT_Info *)data;

  printf("Contents of the first block:\n");
  printf("fileType: %s\n", info_ptr->fileType);
  printf("fileName: %s\n", info_ptr->fileName);
  printf("hash_field: %s\n", info_ptr->hash_field);
  printf("globalDepth: %d\n", info_ptr->globalDepth);
  printf("fileDesc: %d\n", info_ptr->fileDesc);
  printf("rec_num: %d\n", info_ptr->total_num_of_recs);
  printf("num_of_blocks: %d\n", info_ptr->num_of_blocks);
  printf("HT_Info_size: %zu\n", sizeof(HT_Info));
}


int power(int num) {
	int two = 1;
  for(int i = 0; i < num; i++)
		two *= 2;
	return two;
}

void HashTable_resize(int **hash_table, HT_Info *info) {
  printf("\n\n- - - - - - - HASH TABLE RESIZING - - - - - - -\n\n");
  int newSize = power(info->globalDepth);

  int *new_hash_table = (int *)malloc(newSize * sizeof(int));
  
  for (int i = 0; i < newSize; i++) {
    if (i % 2 == 0 && i > 0) {
      for(int j = i-1; j < newSize; j++) {
        new_hash_table[i] = (*hash_table)[j];
        break;
      }
    }
  }
  new_hash_table[0] = (*hash_table)[0];
  for(int i = 0; i < newSize; i++) {
    if (i % 2 != 0)
      new_hash_table[i] = -1;

  }

  free(*hash_table);
  *hash_table = new_hash_table;
  
  for (int i = 0; i < newSize; i++) {
    printf("new_hash_table[%d] = %d\n", i, new_hash_table[i]);
  }
}



void Print_Hash_Table(int *hash_table, HT_Info *info) {
  int size = power(info->globalDepth);
  printf("\n\n- - - - - HASH TABLE PRINTING - - - - -\n");
  printf("- - - - - - - - - - - - - - - - - - - -\n");

  for (int i = 0; i < size; i++) {
    if (hash_table[i] != -1) {
      printf("\t   hash_table[%d] = %d\n", i, hash_table[i]);
    }
    else {
      printf("\t   hash_table[%d] = NULL\n", i);
    }
  }

  printf("\n\n");
}