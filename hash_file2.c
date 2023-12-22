#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "bf.h"
#include "hash_file.h"
#define MAX_OPEN_FILES 20


// #define TABLE_SIZE 4
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
static struct Openfiles{
  int file_desc;
  int *hashTable;
  BF_Block *info;
} openFiles[MAX_OPEN_FILES];


HT_ErrorCode HT_Init() {
  BF_Init(LRU);
  // Αρχικοποιούμε κάθε θέση του πίνακα.
  for (int i = 0; i < MAX_OPEN_FILES; ++i) {
    openFiles[i].file_desc = -1;
    
    if (i == 0) {
      // Δεσμεύουμε δυναμικά μνήμη ανάλογα με το TABLE_SIZE.
      openFiles[i].hashTable = (int *)malloc(TABLE_SIZE * sizeof(int));
      printf("\n\narxikh dieuthinsi hash table: %p\n\n", openFiles[i].hashTable);

      if (openFiles[i].hashTable == NULL) {
        return HT_ERROR;
      }
      for (int j = 0; j < TABLE_SIZE; ++j) {
        openFiles[i].hashTable[j] = -1;
        // printf("openFiles[%d].hashTable[%d] = %d\n", i, j, openFiles[i].hashTable[j]);
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
  CALL_BF(BF_OpenFile(filename, &file_desc));

  // Ορίζουμε, αρχικοποιούμε και δεσμεύουμε ένα block, για τα μεταδεδομένα του αρχείου.
  BF_Block *infoBlock;
  BF_Block_Init(&infoBlock);
  CALL_BF(BF_AllocateBlock(file_desc, infoBlock));
  
  HT_Info info;

  // Ορίζουμε έναν δείκτη που δείχνει στα δεδομένα του block των μεταδεδομένων.
  void *infoData = BF_Block_GetData(infoBlock);
        
  // Περνάμε τα μεταδεδομένα στο 1ο block του αρχείου.      
  strncpy(info.fileType, "Hash File", sizeof(info.fileType));
  info.fileType[sizeof(info.fileType) - 1] = '\0';
  
  strncpy(info.fileName, filename, sizeof(info.fileName));
  info.fileName[sizeof(info.fileName) - 1] = '\0';
  
  strncpy(info.hash_field, "id", sizeof(info.hash_field));
  info.hash_field[sizeof(info.hash_field) - 1] = '\0';

  info.total_num_of_recs = 0;
  info.num_of_blocks = 1;
  info.globalDepth = depth;
  info.hashTable_size = TABLE_SIZE;

  memset(infoData, 0, BF_BLOCK_SIZE);
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
  // Find an available slot in the open_files array
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

  // Initialising the "info" block
  BF_Block_Init(&openFiles[*indexDesc].info);

  // Getting the block in the global array.
  CALL_BF(BF_GetBlock(openFiles[*indexDesc].file_desc, 0, openFiles[*indexDesc].info));
  return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc) {
  // Ελέγχουμε αν ο δεικτής που μας δόθηκε αντιστοιχεί σε κάποια θέση θέση του πίνακα,
  // κι αν ναι, ελέγχουμε αν αυτή η θέση περιέχει κάποιο αρχείο.
  if (indexDesc < 0 || indexDesc >= MAX_OPEN_FILES || openFiles[indexDesc].file_desc == -1) {
    return HT_ERROR;
  }

  HT_Info* info = (HT_Info *)BF_Block_GetData(openFiles[indexDesc].info);
  
  
  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  /* * * * *  Αποθήκευση του πίνακα κατακερματισμού στο αρχείο.  * * * * */
  /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  
  // Ένα block τύπου BF_Block* μπορεί να αποθηκεύσει BF_BLOCK_SIZE / sizeof(int) θέσεις πίνακα.
  // Στην προκειμένη περίπτωση 512 / 4 = 128.
  // Ο πίνακας κατακερματισμού μας αποτελείται από info->hashTable_size θέσεις.
  // Άρα, θα χρειαστούν info->hashTable_size / [(BF_BLOCK_SIZE-4) / sizeof(int)] blocks για την αποθηκευσή του.
  // Στα 4 πρώτα bytes αποθηκέυουμε το είδος του block ('b' for bucket block, 'h' for hash table block).

  double hashTableBlocks = info->hashTable_size / (double)((BF_BLOCK_SIZE - 4) / sizeof(int));
  int hashBlocks = ceil(hashTableBlocks);
  
  printf("\n\n\n\nThe Hash Table needs %d blocks to be stored.\n\n\n", hashBlocks);
  char h = 'h';
  BF_Block *block;
  HashStoring hashArray;
  void *data;

//   int k = 0;
//   int count = 0;
//   for(int i = 0; i < hashBlocks; i++) {
//     BF_Block_Init(&block);
//     CALL_BF(BF_AllocateBlock(openFiles[indexDesc].file_desc, block));
//     data = BF_Block_GetData(block);
//     printf("Hash Block Num: %d, data address: %p, INFO ADDRESS: %p\n\n", i, data, info);

//     memset(data, 0, BF_BLOCK_SIZE);
//     memcpy(data, &h, sizeof(char));

//     // int count = 0;
//     int l = 0;
//     for (int j = 4; j < BF_BLOCK_SIZE && l < info->hashTable_size; j += sizeof(int)) {
//     if (openFiles[indexDesc].hashTable != NULL) {
//         hashArray.hashTable[l] = openFiles[indexDesc].hashTable[l + k];
//     } else {
//         printf("Malakia!\n");
//     }
//     l++;
//     count++;
// }

    

//     BF_Block_SetDirty(block);
//     CALL_BF(BF_UnpinBlock(block));
//     BF_Block_Destroy(&block);

//     if(info->hashTable_size-count>0)
//       k += count;
//     else
//       k = info->hashTable_size-count;
//   }

  BF_Block_SetDirty(openFiles[indexDesc].info);
  CALL_BF(BF_UnpinBlock(openFiles[indexDesc].info));
  BF_Block_Destroy(&openFiles[indexDesc].info);

  // free(openFiles[indexDesc].info);
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
  int fd = openFiles[indexDesc].file_desc;
  //// printf("\n\nI N S E R T \n\n");

  // Ο δείκτης *info δείχνει στα μεταδεδομένα του 1ου block.
  HT_Info *info = (HT_Info *)BF_Block_GetData(openFiles[indexDesc].info);

  // Εκτύπωση του πίνακα κατακερατισμού.
  //// Print_Hash_Table(openFiles[indexDesc].hashTable, info);

  // Υπολογίζουμε την τιμή κατακερματισμού του εκάστοτε record.id.
  int hashValue = hash(record.id, info->globalDepth);
  if(info->hashTable_size < power(info->globalDepth))
    HashTable_resize(&openFiles[indexDesc].hashTable, info);


  BF_Block *bucket;           // Το bucket στο οποίο αντστοιχεί το record, με βάση το αρχικό hashvalue του, άσχετα με το αν χωράει σε αυτό.
  BF_Block *newBucket;        // Σε περίπτωση που χρειαστεί να δημιουργήσυμε καινούριο bucket.
  Block_Info blockInfo;
  void *bucketData;
  void *newBucketData;

  if(openFiles[indexDesc].hashTable[hashValue] == -1) {
    BF_Block_Init(&bucket);
    CALL_BF(BF_AllocateBlock(fd, bucket));

    // Ο δείκτης *bucketData δείχνει στα δεδομένα του block που μόλις δεσμεύσαμε.
    bucketData = BF_Block_GetData(bucket);

    // Για ασφάλεια, πριν περάσουμε τον αναγνωριστικό αριθμό (block_id) στο καινούριο block,
    // χρησιμοποιούμε την BF_GetBlockCounter(), ενημερώνοντας ταυτόχρονα και το info->num_of_blocks.
    CALL_BF(BF_GetBlockCounter(fd, &info->num_of_blocks));
    blockInfo.block_id = info->num_of_blocks - 1;
    openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
    
    strncpy(blockInfo.block_type, "b", sizeof(char));
    blockInfo.block_type[sizeof(blockInfo.block_type) - 1] = '\0';

    blockInfo.bucket_size = 0;
    // blockInfo.local_depth = info->globalDepth;
    blockInfo.local_depth = 1;
    blockInfo.buddies = 0;

    // "Καθαρίζουμε" το καινούριο block από τυχόντα σκουπίδια που περιείχε η μνήμη.
    memset(bucketData, 0, BF_BLOCK_SIZE);

    // Περνάμε το blockInfo στην αρχή του block.
    memcpy(bucketData, &blockInfo, sizeof(Block_Info));   // Για λόγους συμμετρίας η κεφαλίδα του block, δεσμεύσει χώρο όσο μία εγγραφή.
  }
  else {
    // Φέρνουμε το bucket στην μνήμη καθώς και τα δεδομένα του στον δείκτη *bucketData.
    BF_Block_Init(&bucket);
    CALL_BF(BF_GetBlock(fd, openFiles[indexDesc].hashTable[hashValue], bucket));
    bucketData = BF_Block_GetData(bucket);
  }
  Block_Info *ptr = (Block_Info *)bucketData;
  // Η εγγραφή χωράει στο bucket που της αντιστοιχεί.
  if(BF_BLOCK_SIZE > (ptr->bucket_size + 2) * sizeof(Record)) {       // Κάνουμε "ptr->bucket_size + 2", γιατί: + 1 το Block_Info, + 1 το record προς εισαγωγή.
    Record *recordinBlock = (Record *)malloc(sizeof(Record));
    recordinBlock = ((void *)bucketData + ((ptr->bucket_size + 1) * sizeof(Record)));


    // Αντιγράφουμε τα δεδομένα του record στα περιεχόμενα του δείκτη *recordinBlock,
    // που δείχνει στην διεύθυνση της επόμενης εγγραφής.
    recordinBlock->id = record.id;
    strncpy(recordinBlock->name, record.name, sizeof(recordinBlock->name));
    recordinBlock->name[sizeof(recordinBlock->name) - 1] = '\0';            // Χαρακτήρας "τέλος κειμένου"
    strncpy(recordinBlock->surname, record.surname, sizeof(recordinBlock->surname));
    recordinBlock->surname[sizeof(recordinBlock->surname) - 1] = '\0';      // Χαρακτήρας "τέλος κειμένου"
    strncpy(recordinBlock->city, record.city, sizeof(recordinBlock->city));
    recordinBlock->city[sizeof(recordinBlock->city) - 1] = '\0';            // Χαρακτήρας "τέλος κειμένου"


    // Αυξάνουμε το bucket_size, και το total_num_of_recs
    ptr->bucket_size ++;
    info->total_num_of_recs ++;
    // printf("RECORD TO BE INSERTED:\nhash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, recordinBlock->id, recordinBlock->name, recordinBlock->surname, recordinBlock->city);
    // printf("Records inside bucket (id: %d): %d\n\n", ptr->block_id, ptr->bucket_size);
    
    // Κάνουμε το bucket (block) dirty, unpin και destroy, αφού δεν θα το χρειαστούμε άλλο
    BF_Block_SetDirty(bucket);
    CALL_BF(BF_UnpinBlock(bucket));
    BF_Block_Destroy(&bucket);

    ptr = NULL;
    return HT_OK;
  }
  else {
    // Τ Ο Π Ι Κ Ο  Β Α Θ Ο Σ  ==  Ο Λ Ι Κ Ο  Β Α Θ Ο Σ
    if(ptr->local_depth == info->globalDepth) {// Σε αυτήν την περίπτωση η εγγραφή δεν χωράει στο block.
      // printf("\nL O C A L  D E P T H  ==  G L O B A L  D E P T H\n\n");
      // (1) Διπλασιάζουμε τον πίνακα κατακερματισμού.
      // printf("Eggrafh pros eisagwgh:\nhash: %d, ID: %d, name: %s, surname: %s, city: %s\n\nPalies eggrafes:\n", hashValue, record.id, record.name, record.surname, record.city);

      // Αυξάνουμε το ολικό βαθός του πίνακα και το τοπικό βάθος του κουβά.
      info->globalDepth ++;
      ptr->local_depth ++;
      
      // Διπλασιάζουμε τον πίνακα κατακερματισμού.
      HashTable_resize(&openFiles[indexDesc].hashTable, info);

      // Αρχικοποιούμε και δεσμεύουμε ένα καινούριο bucket για την νέα εγγραφή.
      BF_Block_Init(&newBucket);
      CALL_BF(BF_AllocateBlock(fd, newBucket));
      // Φέρνουμε τα δεδομένα στον δείκτη.
      newBucketData = BF_Block_GetData(newBucket);

      // Αρχικοποιούμε τις τιμές.
      CALL_BF(BF_GetBlockCounter(fd, &info->num_of_blocks));
      blockInfo.block_id = info->num_of_blocks-1;
      
      strncpy(blockInfo.block_type, "b", sizeof(char));
      blockInfo.block_type[sizeof(blockInfo.block_type) - 1] = '\0';

      blockInfo.bucket_size = 0;
      blockInfo.buddies = 0;
      blockInfo.local_depth = ptr->local_depth;

      // Σε κάθε διπλασιασμό του πίνακα κατακερματισμού δημιουργείται το κελί cell<<1 και το κελί (cell<<1)+1.
      // Η διεύθυνση του καινούριου μπλοκ που δεσμεύουμε είναι πάντα στην θέση (cell<<1)+1.
      if(hashValue & 1 == 1)
        openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
      else
        openFiles[indexDesc].hashTable[(hashValue<<1)+1] = blockInfo.block_id;


      // printf("openFiles[0].hashTable[%d]: %d\n\n ", hashValue, openFiles[indexDesc].hashTable[hashValue]);
      int numOfPlacesInTheTable = power(info->globalDepth);
      blockInfo.block_id = info->num_of_blocks - 1;
      for(int l = 0; l < numOfPlacesInTheTable; l ++)
        if(l%2 != 0 && (openFiles[indexDesc].hashTable[l] == -1)) {
          openFiles[indexDesc].hashTable[l] = openFiles[indexDesc].hashTable[l-1];
            if(openFiles[indexDesc].hashTable[l-1] == blockInfo.block_id)
              blockInfo.buddies++;
        }
      

      // Print_Hash_Table(openFiles[indexDesc].hashTable, info);


      memset(newBucketData, 0, sizeof(Record));
      memcpy(newBucketData+1, &blockInfo, sizeof(Block_Info));
    }
    // Τ Ο Π Ι Κ Ο  Β Α Θ Ο Σ  <  Ο Λ Ι Κ Ο  Β Α Θ Ο Σ
    else if(ptr->local_depth < info->globalDepth) {
      // printf("L O C A L  D E P T H  <  G L O B A L  D E P T H\n\n");
      ptr->local_depth++;

      BF_Block_Init(&newBucket);
      CALL_BF(BF_AllocateBlock(fd, newBucket));
      newBucketData = BF_Block_GetData(newBucket);

      CALL_BF(BF_GetBlockCounter(fd, &info->num_of_blocks));
      blockInfo.block_id = info->num_of_blocks-1;
      
      strncpy(blockInfo.block_type, "b", sizeof(char));
      blockInfo.block_type[sizeof(blockInfo.block_type) - 1] = '\0';

      blockInfo.bucket_size = 0;
      blockInfo.buddies = 0;
      blockInfo.local_depth = ptr->local_depth;
      openFiles[indexDesc].hashTable[hashValue] = blockInfo.block_id;
      // Print_Hash_Table(openFiles[indexDesc].hashTable, info);

      int numOfPlacesInTheTable = power(info->globalDepth);
      for(int l = 0; l < numOfPlacesInTheTable; l ++) {
        BF_Block *buddies;

        if(openFiles[indexDesc].hashTable[l] == blockInfo.block_id ) {
          BF_Block_Init(&buddies);
          CALL_BF(BF_GetBlock(fd, openFiles[indexDesc].hashTable[l], buddies));
          Block_Info *buddiesData = (Block_Info *)BF_Block_GetData(buddies);
            if(blockInfo.buddies < buddiesData->buddies) {
              openFiles[indexDesc].hashTable[l] = blockInfo.block_id;
              buddiesData->buddies--;
              blockInfo.buddies++;
              
              BF_Block_SetDirty(buddies);
            }
          CALL_BF(BF_UnpinBlock(buddies));
          BF_Block_Destroy(&buddies);
        }
      }

      memset(newBucketData, 0, sizeof(Record));
      memcpy(newBucketData, &blockInfo, sizeof(Block_Info));
    }
    Block_Info *ptrNew = (Block_Info *)newBucketData;
    Record *records;
    Block_Info *currentBucket;
    void *data;

    int temp = ptr->bucket_size+1;
    int fakeSizeForOldBucket = ptr->bucket_size;
    ptr->bucket_size = 0;
    Record recursionRec;
    int oldBucket_id = ptr->block_id;
    int newBucket_id = blockInfo.block_id;
    for(int i = 0; i < temp; i++) {
      int flag = 0;
      if(i < temp - 1) {
        // Διαβάζουμε μία-μία τις εγγραφές που περίεχονατν ήδη στον κάδο.
        records = (Record *)(bucketData + sizeof(Record) * (i+1));
        // Επανυπολογίζουμε την τιμή κατακερματισμού με το καινούριο βάθος.
        hashValue = hash(records->id, info->globalDepth);
      }
      else {
        // Μόλις τελειώσουμε με τις ήδη υπάρχουσες εγγραφές,
        // εξετάζουμε την τιμή κατακερματισμού της δοθείσας.
        hashValue = hash(record.id, info->globalDepth);
        records = &record;
        info->total_num_of_recs ++;
      }

      // Αναλόγως το hash value της εγγραφής οι δείκτες data και currentBucket δείχνουν
      // στα δεδομένα του bucket στα οποία θα καταλήξει η εγγραφή, και στις πληροφορίες κεφαλίδας.
      if(oldBucket_id == openFiles[indexDesc].hashTable[hashValue] && (BF_BLOCK_SIZE > (ptr->bucket_size + 2) * sizeof(Record))) {
        data = bucketData;
        currentBucket = ptr;
      }
      else if(newBucket_id == openFiles[indexDesc].hashTable[hashValue] && (BF_BLOCK_SIZE > (ptrNew->bucket_size + 2) * sizeof(Record))){ 
        data = newBucketData;
        currentBucket = ptrNew;
      }
      else {
        HT_InsertEntry(indexDesc, *records);  
        
        // Χρησιμοποιούμε την μεταβλητή flag για να ελέγξουμε αν πραγματοποιήθηκε αναδρομική κλήση της HT_InsertEntry().
        flag = 1;
      }
      // Ελέγχουμε αν για την συγκεκριμένη εγγραφή πραγματοποιήθηκε αναδρομή.
      // Σε περίπτωση που έγινε, η εγγραφή βρίσκεται ήδη στον κουβα που της αντιστοιχεί,
      // άρα αγνοούμε το "if".
      if(!flag) {
        // Δεσμεύουμε τον κατάλληλο χώρο για να χωρέσουμε την εγγραφή.
        Record *recordinBlock = (Record *)malloc(sizeof(Record));
        recordinBlock = ((void *)data + ((currentBucket->bucket_size + 1) * sizeof(Record)));


        // Αντιγράφουμε τα δεδομένα του record στα περιεχόμενα του δείκτη *recordinBlock,
        // που δείχνει στην διεύθυνση της επόμενης εγγραφής.
        if(recordinBlock != records) {      // Ελέγχουμε αν η εγγραφή προόριζεται για την θέση στην οποία βρίσκεται ήδη.
          recordinBlock->id = records->id;
          strncpy(recordinBlock->name, records->name, sizeof(recordinBlock->name));
          recordinBlock->name[sizeof(recordinBlock->name) - 1] = '\0';            // Χαρακτήρας "τέλος κειμένου"
          strncpy(recordinBlock->surname, records->surname, sizeof(recordinBlock->surname));
          recordinBlock->surname[sizeof(recordinBlock->surname) - 1] = '\0';      // Χαρακτήρας "τέλος κειμένου"
          strncpy(recordinBlock->city, records->city, sizeof(recordinBlock->city));
          recordinBlock->city[sizeof(recordinBlock->city) - 1] = '\0';            // Χαρακτήρας "τέλος κειμένου"
        }
        currentBucket->bucket_size ++;

        // printf("RECORD TO BE INSERTED:\nhash: %d, ID: %d, name: %s, surname: %s, city: %s\n", hashValue, records->id, records->name, records->surname, records->city);
        // printf("Records inside bucket (id: %d): %d\n\n", currentBucket->block_id, (currentBucket->bucket_size));
      }
    }
  }
  BF_Block_SetDirty(newBucket);
  CALL_BF(BF_UnpinBlock(newBucket));
  BF_Block_Destroy(&newBucket);

  BF_Block_SetDirty(bucket);
  CALL_BF(BF_UnpinBlock(bucket));
  BF_Block_Destroy(&bucket);

  return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {
    // Έλεγχος αν το αρχείο είναι ανοιχτό
    if (openFiles[indexDesc].file_desc == -1) {
        printf("Error: File is not open.\n");
        return HT_ERROR;
    }

    // Λήψη του block που περιέχει τα μεταδεδομένα
    BF_Block* infoBlock;
    BF_Block_Init(&infoBlock);
    CALL_BF(BF_GetBlock(openFiles[indexDesc].file_desc, 0, infoBlock));

    // Παίρνω τα μεταδεδομένa
    HT_Info* info = (HT_Info*)BF_Block_GetData(infoBlock);

    // Επανάληψη για κάθε block και εκτύπωση των records
    for (int i = 1; i < info->num_of_blocks; ++i) {
        BF_Block* block;
        BF_Block_Init(&block);

        // Λήψη του block που περιέχει τα records
        CALL_BF(BF_GetBlock(openFiles[indexDesc].file_desc, i, block));

        // Παίρνω το block info
        Block_Info* blockInfo = (Block_Info*)BF_Block_GetData(block);
        printf("block id: %d, address: %p\n\n", blockInfo->block_id, blockInfo);
        if(id == NULL) {
          printf("\nBlock ID: %d\n", blockInfo->block_id);
          printf("Local Depth: %d\n", blockInfo->local_depth);
          printf("Bucket Size: %d\n", blockInfo->bucket_size);
          // printf("Buddies: %d\n", blockInfo->buddies);
        }
        // Επανάληψη για κάθε record στο block και εκτύπωση
        for (int j = 1; j <= blockInfo->bucket_size; ++j) {
            Record* record = (Record*)((char*)blockInfo + j * sizeof(Record));
            // Βρίσκω το record με το συγκεκριμένο ID και το εκτυπώνω
            if (id == NULL || *id == record->id) {
              printf("ID: %d, Name: %s, Surname: %s, City: %s\n", record->id, record->name, record->surname, record->city);
            }        
            
          }

        // Unpin και καταστροφή του block
        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
    }

    // Unpin και καταστροφή του block μεταδεδομένων
    CALL_BF(BF_UnpinBlock(infoBlock));
    BF_Block_Destroy(&infoBlock);

    return HT_OK;
}

HT_ErrorCode HashStatistics(char* filename) {
  // Ανοίγω το αρχείο με όνομα filename
  int fd;
  BF_Block* infoBlock;
  BF_Block_Init(&infoBlock); 
  HT_Info* info;
  CALL_BF(BF_OpenFile(filename, &fd));

  // Παίρνω το info block (πρώτο block)
  CALL_BF(BF_GetBlock(fd, 0, infoBlock));
  info = (HT_Info*)BF_Block_GetData(infoBlock);

  // Εκτυπώνω τον συνολικό αριθμό των block
  printf("Total number of blocks: %d", info->num_of_blocks);

  int totalRecords = 0;
  int minRecords = 9; // μεγιστος αριθμος 7+1
  int maxRecords = 0;

  // Επαναλαμβάνω για κάθε block (εκτός του πρώτου που είναι το info block)
  for (int i = 1; i < info->num_of_blocks; ++i) {
    BF_Block* block;
    BF_Block_Init(&block);

    // Παίρνω το block που περιέχει τα records
    CALL_BF(BF_GetBlock(fd, i, block));
    Block_Info* blockInfo = (Block_Info*)BF_Block_GetData(block);


    // Ενημερώνω τον συνολικό αριθμό των εγγραφών
    totalRecords += blockInfo->bucket_size;

    // Ενημερώνω τους ελάχιστους και μέγιστους αριθμούς εγγραφών
    if (blockInfo->bucket_size < minRecords) {
      minRecords = blockInfo->bucket_size;
    }
    if (blockInfo->bucket_size > maxRecords) {
      maxRecords = blockInfo->bucket_size;
    }

    // Απελευθερώνει το block
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
  }

  // Υπολογίζω και εκτυπώνω τον μέσο όρο εγγραφών ανά bucket
  double averageRecords = (double)totalRecords / (info->num_of_blocks - 1);
  printf("\nMinimum Records per Bucket: %d\n", minRecords);
  printf("Average Records per Bucket: %.2lf\n", averageRecords);
  printf("Maximum Records per Bucket: %d\n", maxRecords);

  // Απελευθερώνω το info block
  CALL_BF(BF_UnpinBlock(infoBlock));
  BF_Block_Destroy(&infoBlock);
 
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

void HashTable_resize(int** hash_table, HT_Info *info) {
  // printf("\n\n- - - - - - - HASH TABLE RESIZING - - - - - - -\n\n");
  int newSize = power(info->globalDepth);

  int *new_hash_table = (int *)malloc(newSize * sizeof(int));
  
  int i = 0;
    for (int j = 0; j < newSize / 2; j ++) {
      new_hash_table[i] = (*hash_table)[j];
      i += 2 ;
    }


    for(int i = 0; i < newSize; i++) {
      if (i % 2 != 0)
        new_hash_table[i] = -1;

    }

  info->hashTable_size = newSize;

  printf("old hash table address: %p\n", *hash_table);
  printf("new hash table address: %p\n\n", new_hash_table);

  free(*hash_table);
  *hash_table = new_hash_table;

  
  // for (int i = 0; i < newSize; i++) {
  //   printf("new_hash_table[%d] = %d\n", i, new_hash_table[i]);
  // }
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