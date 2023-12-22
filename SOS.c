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

  // Ο δείκτης *info δείχνει στα μεταδεδομένα του 1ου block.
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

  // > 1η κλήση της συνάρτησης.
  // Δημιουργία 1ου bucket.
  if(info->num_of_blocks == 1 && openFiles[indexDesc].hashTable[hashValue] == -1) {
    // Αρχικοποιύμε και δεσμεύουμε ένα block.
    BF_Block_Init(&bucket);
    CALL_BF(BF_AllocateBlock(info->fileDesc, bucket));

    // Ο δείκτης *bucketData δείχνει στα δεδομένα του block που μόλις δεσμεύσαμε.
    bucketData = BF_Block_GetData(bucket);

    // Για ασφάλεια, πριν περάσουμε τον αναγνωριστικό αριθμό (block_id) στο καινούριο block,
    // χρησιμοποιούμε την BF_GetBlockCounter(), ενημερώνοντας ταυτόχρονα και το info->num_of_blocks.
    CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
    blockInfo.block_id = info->num_of_blocks - 1;
    openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
    blockInfo.bucket_size = 0;
    blockInfo.local_depth = 1;
    blockInfo.buddiesBoolean = 0;

    // "Καθαρίζουμε" το καινούριο block από τυχόντα σκουπίδια που περιείχε η μνήμη.
    memset(bucketData, 0, sizeof(Record));

    // Περνάμε το blockInfo στην αρχή του block.
    memcpy(bucketData, &blockInfo, sizeof(Block_Info));   // Για λόγους συμμετρίας η κεφαλίδα του block, δεσμεύσει χώρο όσο μία εγγραφή.
  }
  // > Υπάρχει θέση με αυτό το hash value στον πίνακα κατακερματισμού, αλλά δεν της αντιστοιχεί κάποιο block ακόμα.
  else {
    // Συμβαίνει μόνο στο 2ο bucket.
    if(openFiles[indexDesc].hashTable[hashValue] == -1) {
        // Αρχικοποιούμε και δεσμεύουμε ένα καινούριο bucket για το καινούριο hashvalue.
        BF_Block_Init(&bucket);
        CALL_BF(BF_AllocateBlock(info->fileDesc, bucket));

        // Ο δείκτης *bucketData δείχνει στα δεδομένα του block που μόλις δεσμεύσαμε.
        bucketData = BF_Block_GetData(bucket);


        // Για ασφάλεια, πριν περάσουμε τον αναγνωριστικό αριθμό (block_id) στο καινούριο block,
        // χρησιμοποιούμε την BF_GetBlockCounter(), ενημερώνοντας ταυτόχρονα και το info->num_of_blocks.
        CALL_BF(BF_GetBlockCounter(info->fileDesc, &info->num_of_blocks));
        blockInfo.block_id = info->num_of_blocks - 1;
        openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
        blockInfo.bucket_size = 0;
        blockInfo.local_depth = 1;
        blockInfo.buddiesBoolean = 0;

        // "Καθαρίζουμε" το καινούριο block από τυχόντα σκουπίδια που περιείχε η μνήμη.
        memset(bucketData, 0, sizeof(Record));

        // Περνάμε το blockInfo στην αρχή του block.
        memcpy(bucketData, &blockInfo, sizeof(Block_Info));   // Για λόγους συμμετρίας η κεφαλίδα του block, δεσμεύσει χώρο όσο μία εγγραφή.
    }
    // Σε αυτήν την περίπτωση η θέση του πίνακα με τιμή "hashValue", "δείχνει" την διεύθυνση κάποιου bucket.
    else {
      // Φέρνουμε το bucket στην μνήμη καθώς και τα δεδομένα του στον δείκτη *bucketData.
      BF_Block_Init(&bucket);
      CALL_BF(BF_GetBlock(info->fileDesc, openFiles[indexDesc].hashTable[hashValue], bucket));
      bucketData = BF_Block_GetData(bucket);
    }
  }
  Block_Info *ptr = (Block_Info *)bucketData;

  
  // Η εγγραφή χωράει στο bucket που της αντιστοιχεί.
  if(BF_BLOCK_SIZE > (ptr->bucket_size + 2) * sizeof(Record)) {       // Κάνουμε "ptr->bucket_size + 2", γιατί: + 1 το Block_Info, + 1 το record προς εισαγωγή.
    Record* recordinBlock = (Record*)malloc(sizeof(Record));
    recordinBlock = (Record *)(bucketData + ((ptr->bucket_size + 1) * sizeof(Record)));


    // Αντιγράφουμε τα δεδομένα του record στα περιεχόμενα του δείκτη *recordinBlock,
    // που δείχνει στην διεύθυνση της επόμενης εγγραφής.
    recordinBlock->id = record.id;

    strncpy(recordinBlock->name, record.name, sizeof(recordinBlock->name));
    recordinBlock->name[sizeof(recordinBlock->name) - 1] = '\0';  // Χαρακτήρας "τέλος κειμένου"

    strncpy(recordinBlock->surname, record.surname, sizeof(recordinBlock->surname));
    recordinBlock->surname[sizeof(recordinBlock->surname) - 1] = '\0';  // Χαρακτήρας "τέλος κειμένου"

    strncpy(recordinBlock->city, record.city, sizeof(recordinBlock->city));
    recordinBlock->city[sizeof(recordinBlock->city) - 1] = '\0';  // Χαρακτήρας "τέλος κειμένου"


    // Αυξάνουμε το bucket_size, και το total_num_of_recs
    ptr->bucket_size ++;
    info->total_num_of_recs ++;
    printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, recordinBlock->id, recordinBlock->name, recordinBlock->surname, recordinBlock->city);
    printf("Eggrafes mexri stigmhs sto bucket %d: %d\n\n", hashValue, ptr->bucket_size);
    
    // Κάνουμε το bucket (block) dirty, unpin και destroy, αφού δεν θα το χρειαστούμε άλλο
    BF_Block_SetDirty(bucket);
    BF_Block_SetDirty(infoBlock);

    CALL_BF(BF_UnpinBlock(bucket));
    BF_Block_Destroy(&bucket);

    CALL_BF(BF_UnpinBlock(infoBlock));
    BF_Block_Destroy(&infoBlock);
    ptr = NULL;
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
        }


      
      blockInfo.bucket_size = 0;
      blockInfo.buddiesBoolean = 0;
      blockInfo.local_depth = ptr->local_depth;

      Print_Hash_Table(openFiles[indexDesc].hashTable, info);


      memset(newBucketData, 0, sizeof(Record));
      memcpy(newBucketData, &blockInfo, sizeof(Block_Info));

      Block_Info *ptrNew = (Block_Info *)newBucketData;
      

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
        if(oldBucket_id == openFiles[indexDesc].hashTable[hashValue] && (BF_BLOCK_SIZE > (ptr->bucket_size + 2) * sizeof(Record))) {
          data = bucketData;
          currentBucket = ptr;
          currentSize = &ptr->bucket_size;
          printf("MPHKE STO %d\n", oldBucket_id);
        }
        else if(newBucket_id == openFiles[indexDesc].hashTable[hashValue] && (BF_BLOCK_SIZE > (ptrNew->bucket_size + 2) * sizeof(Record))){ 
          data = newBucketData;
          currentBucket = ptrNew;
          currentSize = &ptrNew->bucket_size;
          printf("MPHKE STO %d\n", newBucket_id);
        }
        else {
          printf("- - - A N A D R O M H - - -\n");

          // blockInfo.local_depth = info->globalDepth;
          
          // memcpy(newBucketData, &blockInfo, sizeof(Block_Info));
          BF_Block_SetDirty(newBucket);
          CALL_BF(BF_UnpinBlock(newBucket));
          BF_Block_Destroy(&newBucket);

      
          BF_Block_SetDirty(bucket);
          CALL_BF(BF_UnpinBlock(bucket));
          BF_Block_Destroy(&bucket);

          BF_Block_SetDirty(infoBlock);
          CALL_BF(BF_UnpinBlock(infoBlock));
          BF_Block_Destroy(&infoBlock);

          HT_InsertEntry(indexDesc, *records);  
          flag = 1;

          BF_Block_Init(&infoBlock);
          CALL_BF(BF_GetBlock(openFiles[indexDesc].file_desc, 0, infoBlock));
          info = (HT_Info *)BF_Block_GetData(infoBlock);

          BF_Block_Init(&bucket);
          CALL_BF(BF_GetBlock(info->fileDesc, oldBucket_id, bucket));
          bucketData = BF_Block_GetData(bucket);
          ptr = (Block_Info *)bucketData;

          
          BF_Block_Init(&newBucket);
          CALL_BF(BF_GetBlock(info->fileDesc, newBucket_id, newBucket));
          bucketData = BF_Block_GetData(newBucket);
          memcpy(&blockInfo, newBucketData, sizeof(Block_Info));
    
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


          memset(currentRecord, 0, sizeof(Record));
          printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, records->id, records->name, records->surname, records->city);
          *currentSize += 1;
          printf("Eggrafes mexri stigmhs sto bucket me id %d: %d\n\n", currentBucket->block_id, (*currentSize));
        }
        ptr->local_depth = info->globalDepth;
        ptrNew->local_depth = info->globalDepth;
      }

      BF_Block_SetDirty(newBucket);
      CALL_BF(BF_UnpinBlock(newBucket));
      BF_Block_Destroy(&newBucket);

  
      BF_Block_SetDirty(bucket);
      CALL_BF(BF_UnpinBlock(bucket));
      BF_Block_Destroy(&bucket);

      BF_Block_SetDirty(infoBlock);
      CALL_BF(BF_UnpinBlock(infoBlock));
      BF_Block_Destroy(&infoBlock);

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
      blockInfo.block_id = info->num_of_blocks - 1;
      
      newBucket_id = blockInfo.block_id;  // might need it in recursion

      openFiles[indexDesc].hashTable[hashValue] = newBucket_id;
      printf("\n- ! - ! - ! - ! - ! - ! - ! - ! - ! -\n");
      printf("openFiles[0].hashTable[%d]: %d, ptr->local_depth: %d\n", hashValue, openFiles[indexDesc].hashTable[hashValue], ptr->local_depth);


      ptr->local_depth ++;
      blockInfo.bucket_size = 0;
      blockInfo.buddiesBoolean = 0;
      blockInfo.local_depth = ptr->local_depth;
      Print_Hash_Table(openFiles[indexDesc].hashTable, info);

      printf("\n\nBlock Info struct:\n");printf("blockInfo.bucket_size: %d\n", blockInfo.bucket_size);
      printf("sizeof(blockInfo): %ld\n\n", sizeof(blockInfo));

      memset(newBucketData, 0, sizeof(Record));
      memcpy(newBucketData, &blockInfo, sizeof(Block_Info));

      Block_Info *ptrNew = (Block_Info *)newBucketData;
      
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
        if(oldBucket_id == openFiles[indexDesc].hashTable[hashValue] && (BF_BLOCK_SIZE > (ptr->bucket_size + 2) * sizeof(Record))) {
          data = bucketData;
          currentBucket = ptr;
          currentSize = &ptr->bucket_size;
          printf("MPHKE STO %d\n", oldBucket_id);
        }
        else if(newBucket_id == openFiles[indexDesc].hashTable[hashValue] && (BF_BLOCK_SIZE > (ptrNew->bucket_size + 2) * sizeof(Record))){ 
          data = newBucketData;
          currentBucket = ptrNew;
          currentSize = &ptrNew->bucket_size;
          printf("MPHKE STO %d\n", newBucket_id);
        }
        else {
          printf("- - - A N A D R O M H - - -\n");

          CALL_BF(HT_InsertEntry(indexDesc, *records));
          flag = 1;
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

          memset(currentRecord, 0, sizeof(Record));
          printf("hash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, records->id, records->name, records->surname, records->city);
          *currentSize += 1;
          printf("Eggrafes mexri stigmhs sto bucket me id %d: %d\n\n", currentBucket->block_id, (*currentSize));
        }
      }
      BF_Block_SetDirty(newBucket);
      CALL_BF(BF_UnpinBlock(newBucket));
      BF_Block_Destroy(&newBucket);

  
      BF_Block_SetDirty(bucket);
      CALL_BF(BF_UnpinBlock(bucket));
      BF_Block_Destroy(&bucket);

      BF_Block_SetDirty(infoBlock);
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
  
  // Copy old elements to the new memory location
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

  // Update the hash table pointer to point to the new memory
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