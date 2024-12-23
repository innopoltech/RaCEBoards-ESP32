#include "m_flash_fat.h"

static const char *TAG = "FlashFAT_lib";

static flash_fat_pins_t flash_fat_pins = {0,};

static spi_bus_config_t bus_config  = {0,};
static esp_flash_spi_device_config_t device_config = {0,};
static esp_flash_t* ext_flash = NULL;
static esp_partition_t* fat_partition = NULL;
static esp_vfs_fat_mount_config_t mount_config = {0,};
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

static bool flash_fat_created = false;


esp_err_t flash_fat_mount(flash_fat_pins_t* flash_fat_pins_, BaseType_t core)
{
    if(flash_fat_created)
    {
        ESP_LOGW(TAG, "FLASH FAT is already mount!");
        return ESP_FAIL;
    }

    //Copy pins
    memcpy(&flash_fat_pins, flash_fat_pins_, sizeof(flash_fat_pins_t));

    //Config SPI bus
    bus_config.mosi_io_num = flash_fat_pins.MOSI;
    bus_config.miso_io_num = flash_fat_pins.MISO;
    bus_config.sclk_io_num = flash_fat_pins.SCK;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.isr_cpu_id = core == PRO_CPU_NUM ? INTR_CPU_ID_0 : INTR_CPU_ID_1;

    esp_err_t ret = spi_bus_initialize(FLASH_FAT_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize spi bus");
        goto error;
    }

    //Config flash driver
    device_config.host_id = FLASH_FAT_SPI_HOST;
    device_config.cs_id = 0;
    device_config.cs_io_num = flash_fat_pins.nCS;
    device_config.io_mode = SPI_FLASH_FASTRD;
    device_config.freq_mhz = FLASH_FAT_SCK_FREQ_MHZ;

    ret = spi_bus_add_flash_device(&ext_flash, &device_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize flash driver");
        goto error;
    }

    //Init flash driver
    ret = esp_flash_init(ext_flash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize external Flash: %s (0x%x)", esp_err_to_name(ret), ret);
        goto error;
    }

    //Print ID
    uint32_t id;
    esp_flash_read_id(ext_flash, &id);
    ESP_LOGI(TAG, "Initialized external Flash, size=%lu KB, ID=0x%x", ext_flash->size / 1024, (unsigned int)id);
    
    //Add partition
    ret = esp_partition_register_external(ext_flash, 0, ext_flash->size, FLASH_FAT_PARTITION_NAME, ESP_PARTITION_TYPE_DATA, 
                                            ESP_PARTITION_SUBTYPE_DATA_FAT, (const esp_partition_t**)&fat_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register external Flash: %s (0x%x)", esp_err_to_name(ret), ret);
        goto error;
    }

    // //Erase flash chip
    // esp_partition_erase_range(fat_partition, 0, ext_flash->size); // ~74 sec ?

    //Print partitions - visible only <8 MB ?
    #if FLASH_FAT_LOG
        ESP_LOGI(TAG, "Listing data partitions:");
        esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);

        for (; it != NULL; it = esp_partition_next(it)) {
            const esp_partition_t *part = esp_partition_get(it);
            ESP_LOGI(TAG, "\tpartition '%s', subtype %d, offset 0x%x, size %x kB",
            part->label, part->subtype, (unsigned int)part->address,(unsigned int)(part->size / 1024) );
        }
        esp_partition_iterator_release(it);
    #endif

    //Mount FAT
    mount_config.max_files = 6;
    mount_config.format_if_mount_failed = true;
    mount_config.allocation_unit_size = CONFIG_WL_SECTOR_SIZE;
    ret = esp_vfs_fat_spiflash_mount_rw_wl(FLASH_FAT_MOUNT_POINT, FLASH_FAT_PARTITION_NAME, (const esp_vfs_fat_mount_config_t*)&mount_config, &s_wl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(ret));
        goto error;
    }

    //Print FAT FS size information
    uint64_t bytes_total, bytes_free;
    esp_vfs_fat_info(FLASH_FAT_MOUNT_POINT, &bytes_total, &bytes_free);
    ESP_LOGI(TAG, "FAT FS: %llu kB total, %llu kB free", bytes_total / 1024, bytes_free / 1024);

    #if FLASH_FAT_LOG
        //Print list files information
        DIR *dir = opendir(FLASH_FAT_MOUNT_POINT);
        struct dirent *entry;
        struct stat file_stat;
        char full_path[350];
        int full_size = 0;

        while (1) 
        {
            entry = readdir(dir);
            if(entry == NULL)
                break;

            snprintf(full_path, sizeof(full_path), "%s/%s", FLASH_FAT_MOUNT_POINT, entry->d_name);
            if (stat(full_path, &file_stat) == 0) 
            {
                printf("Name: %s", entry->d_name);
                if (S_ISREG(file_stat.st_mode))
                {
                    printf(" (File, Size: %ld byte)\n", file_stat.st_size);
                    full_size += file_stat.st_size;
                }
                else if (S_ISDIR(file_stat.st_mode))
                    printf(" (Dir)\n");
                else
                    printf(" (?)\n");
            }
        }
        closedir(dir);
        ESP_LOGI(TAG, "Total size of files = %d bytes", full_size);
    #endif

    ESP_LOGI(TAG, "FLASH FAT mount successful");
    flash_fat_created = true;
    return ESP_OK;

error:
    flash_fat_unmount();
    return ESP_FAIL;
}

esp_err_t flash_fat_unmount()
{
    //Unmount FAT FS
    esp_vfs_fat_spiflash_unmount_rw_wl(FLASH_FAT_MOUNT_POINT, s_wl_handle);

    //Unregister partition
    esp_partition_deregister_external(fat_partition);

    //Remove device
    spi_bus_remove_flash_device(ext_flash);

    //Release bus
    spi_bus_free(FLASH_FAT_SPI_HOST);

    ESP_LOGI(TAG, "FLASH FAT unmount successful");
    flash_fat_created = false;
    return ESP_OK;
}


static void flash_fat_test_write_speed_(const char *path , char *data, int size_)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        #if FLASH_FAT_LOG
            ESP_LOGE(TAG, "Failed to open file for speed_test");
        #endif
        return;
    }

    uint64_t time_start = esp_timer_get_time();
    uint64_t time_now   = 0;
    uint64_t time_write = 0;
    uint64_t summ_time  = 0;

    printf("\n\r");
    for(uint16_t i=0;i<FLASH_FAT_SPEED_TEST_CYCLE;i++)
    {
        time_start = esp_timer_get_time();
        fwrite(data, sizeof(char), size_/sizeof(char), f);
        time_now = esp_timer_get_time();

        time_write = time_now - time_start;
        summ_time += time_write;

        //\x1b[A\r - Escape sequence to ascend to the line above 
        printf("\x1b[A\rWrite_block %3d/%d, time = %9llu us, avr speed/s = %7.1f kB/s \n\r", 
                            i+1, FLASH_FAT_SPEED_TEST_CYCLE, (summ_time/(i+1)),  (FLASH_FAT_SPEED_TEST_SIZE/1000.0f)/(((float)summ_time/(i+1))/1000000.0f));
    }

    fclose(f);
}

static void flash_fat_test_read_speed_(const char *path , char *data, int size_)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        #if FLASH_FAT_LOG
            ESP_LOGE(TAG, "Failed to open file for speed_test");
        #endif
        return;
    }

    uint64_t time_start = esp_timer_get_time();
    uint64_t time_now = 0;
    uint64_t time_write = 0;
    uint64_t summ_time = 0;

    uint8_t correct_ = true;

    printf("\n\r");
    for(uint16_t i=0;i<FLASH_FAT_SPEED_TEST_CYCLE;i++)
    {
        time_start = esp_timer_get_time();
        fread(data, sizeof(char), size_/sizeof(char), f);
        time_now = esp_timer_get_time();

        time_write = time_now - time_start;
        summ_time += time_write;

        //\x1b[A\r - Escape sequence to ascend to the line above 
        printf("\x1b[A\rRead_block %3d/%d, time = %9llu us, avr speed/s = %7.1f kB/s \n\r", 
                            i+1, FLASH_FAT_SPEED_TEST_CYCLE, (summ_time/(i+1)),  (FLASH_FAT_SPEED_TEST_SIZE/1000.0)/(((float)summ_time/(i+1))/1000000.0f));

        for(int i=0;i<size_;i++)
            if(data[i] != 0xAA && data[i] != 0xBB)
            {
                ESP_LOGE(TAG, "Read data not valid!!! - %d", (uint8_t)data[i]);
                correct_ = false;
            }  
    }
    #if FLASH_FAT_LOG
        if(!correct_)
            ESP_LOGE(TAG, "Read data not valid!!!");
    #endif

    fclose(f);
}

void flash_fat_test_speed()
{
    /* Test PSRAM and RAM */
    //File
    const char *file_testing_psram = FLASH_FAT_MOUNT_POINT"/t_psram.bin";
    uint32_t size_psram = (uint32_t)(FLASH_FAT_SPEED_TEST_SIZE) * sizeof(char);

    const char *file_testing_ram = FLASH_FAT_MOUNT_POINT"/t_ram.bin";
    uint32_t size_ram = (uint32_t)(FLASH_FAT_SPEED_TEST_SIZE) * sizeof(char);

    //Data
    char* data_psram = heap_caps_malloc (size_psram, MALLOC_CAP_SPIRAM);
    memset((void*)data_psram, 0xAA, size_psram);

    char* data_ram = malloc(size_ram);
    memset((void*)data_ram, 0xBB, size_ram);

    //Test write
    ESP_LOGI(TAG, "Prepare for speed test write from PSRAM, block = %lu kB", (uint32_t)(FLASH_FAT_SPEED_TEST_SIZE/1000.0f));
    flash_fat_test_write_speed_(file_testing_psram, data_psram, size_psram);
    ESP_LOGI(TAG, "Prepare for speed test write from RAM, block = %lu kB", (uint32_t)(FLASH_FAT_SPEED_TEST_SIZE/1000.0f));
    flash_fat_test_write_speed_(file_testing_ram, data_ram, size_ram);

    //Clear data
    memset((void*)data_psram, 0x00, size_psram);
    memset((void*)data_ram, 0x00, size_ram);

    //Test read
    ESP_LOGI(TAG, "Prepare for speed test read to RAM, block = %lu kB", (uint32_t)(FLASH_FAT_SPEED_TEST_SIZE/1000.0f));
    flash_fat_test_read_speed_(file_testing_psram, data_psram, size_psram);
    ESP_LOGI(TAG, "Prepare for speed test read to PSRAM, block = %lu kB", (uint32_t)(FLASH_FAT_SPEED_TEST_SIZE/1000.0f));
    flash_fat_test_read_speed_(file_testing_ram, data_ram, size_ram);

    //Free memory
    free(data_psram);
    free(data_ram);
}

void flash_fat_WriteOnceNextLine()
{   
    char* n = "\n";
    flash_fat_WriteOnce(n);
}

void flash_fat_WriteOnce(char* msg)
{
    FILE *f = fopen(FLASH_FAT_MOUNT_POINT"/l_once.txt", "a");
    if (f == NULL) {
        #if FLASH_FAT_LOG
            ESP_LOGE(TAG, "Failed to open file for log_once");
        #endif
        return;
    }

    //Write
    fprintf(f, "%s", msg);
    fclose(f);
}

void flash_fat_flash_fat_clear_log(uint8_t is_once)
{
    FILE *f = fopen( is_once ? FLASH_FAT_MOUNT_POINT"/l_once.txt" : FLASH_FAT_MOUNT_POINT"/log.txt", "w");
    if (f == NULL) {
        #if FLASH_FAT_LOG
            ESP_LOGE(TAG, "Failed to clear log");
        #endif
        return;
    }
    fclose(f);
}

void flash_fat_WriteContinuous(char* msg, uint8_t enable, uint8_t flush)
{
    static uint8_t enabled = false;
    static FILE *f;

    //Control file
    if(enable == true && enabled == false)
    {
        f = fopen(FLASH_FAT_MOUNT_POINT"/log.txt", "a");
        if (f == NULL) 
        {
            #if FLASH_FAT_LOG
                ESP_LOGE(TAG, "Failed to open file for log");
            #endif
            return;
        }
    }
    if(enable == false && enabled == true)
    {
        fclose(f);
    }

    //Write data
    if(enable == true)
    {
        fprintf(f, "%s", msg);
        if(flush)
            fflush(f);
    }

    enabled = enable;
}