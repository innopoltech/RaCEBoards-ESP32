#include "m_sd_card.h"

static const char *TAG = "SD_card_lib";

esp_err_t ret;
sdmmc_card_t *card;

static sdmmc_host_t* host;
static esp_vfs_fat_sdmmc_mount_config_t mount_config;

static sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

static const char mount_point[] = MOUNT_POINT;

static bool sd_mounted = false;

esp_err_t sdcard_mount(sdmmc_host_t* host_)
{
    host = host_;

    //Config mount
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 16 * 1024;
    
    //Mount filesystem
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host->slot;

    #if SDCARD_ENABLE_INFO
        ESP_LOGI(TAG, "Mounting filesystem");
    #endif
    ret = esp_vfs_fat_sdspi_mount(mount_point, host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        #if SDCARD_ENABLE_ERROR
            if (ret == ESP_FAIL) {

                    ESP_LOGE(TAG, "Failed to mount filesystem. ");
            } else {
                    ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                            "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
            }
        #endif
        return ESP_FAIL;
    }
    else
    {
        #if SDCARD_ENABLE_INFO
            ESP_LOGI(TAG, "Filesystem mounted");
        #endif
    }

    //Print card info
    sdmmc_card_print_info(stdout, card);
    sd_mounted = true;

    return ESP_FAIL;
}

void sdcard_unmount()
{   
    if(sd_mounted == true)
    {
        //Unmount partition
        esp_vfs_fat_sdcard_unmount(mount_point, card);

        #if SDCARD_ENABLE_INFO
            ESP_LOGI(TAG, "Card unmounted");
        #endif
        sd_mounted = false;
    }

}

static void sdcard_test_write_speed_(const char *path , char *data, int size_)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        #ifdef SDCARD_ENABLE_ERROR
            ESP_LOGE(TAG, "Failed to open file for speed_test");
        #endif
        return;
    }

    uint64_t time_start = esp_timer_get_time();
    uint64_t time_now   = 0;
    uint64_t time_write = 0;
    uint64_t summ_time  = 0;

    printf("\n\r");
    for(uint16_t i=0;i<100;i++)
    {
        time_start = esp_timer_get_time();
        fwrite(data, sizeof(char), size_/sizeof(char), f);
        time_now = esp_timer_get_time();

        time_write = time_now - time_start;
        summ_time += time_write;

        //\x1b[A\r - Escape sequence to ascend to the line above 
        printf("\x1b[A\rWrite_block %3d/100, time = %9llu us, avr speed/s = %7.1f kB/s \n\r", 
                            i+1, time_write,  (10.0)/(((float)summ_time/i)/1000000.0f));
    }

    fclose(f);
}

static void sdcard_test_read_speed_(const char *path , char *data, int size_)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        #ifdef SDCARD_ENABLE_ERROR
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
    for(uint16_t i=0;i<100;i++)
    {
        time_start = esp_timer_get_time();
        fread(data, sizeof(char), size_/sizeof(char), f);
        time_now = esp_timer_get_time();

        time_write = time_now - time_start;
        summ_time += time_write;

        //\x1b[A\r - Escape sequence to ascend to the line above 
        printf("\x1b[A\rRead_block %3d/100, time = %9llu us, avr speed/s = %7.1f kB/s \n\r", 
                            i+1, time_write,  (10.0)/(((float)summ_time/i)/1000000.0f));

        for(int i=0;i<size_;i++)
            if(data[i] != 0xAA && data[i] != 0xBB)
            {
                ESP_LOGE(TAG, "Read data not valid!!! - %d", (uint8_t)data[i]);
                correct_ = false;
            }  
    }
    #ifdef SDCARD_ENABLE_ERROR
        if(!correct_)
            ESP_LOGE(TAG, "Read data not valid!!!");
    #endif

    fclose(f);
}

void sdcard_test_speed()
{
    /* Test PSRAM and RAM */
    //File
    const char *file_testing_psram = MOUNT_POINT"/t_psram.bin";
    uint32_t size_psram = 10*1024 * sizeof(char); //10kb

    const char *file_testing_ram = MOUNT_POINT"/t_ram.bin";
    uint32_t size_ram = 10*1024 * sizeof(char); //10kb

    //Data
    char* data_10kB_psram = heap_caps_malloc (size_psram, MALLOC_CAP_SPIRAM);
    memset((void*)data_10kB_psram, 0xAA, size_psram);

    char* data_10kB_ram = malloc(size_ram);
    memset((void*)data_10kB_ram, 0xBB, size_ram);

    //Test write
    ESP_LOGI(TAG, "Prepare for speed test write from PSRAM, block = 10kB");
    sdcard_test_write_speed_(file_testing_psram, data_10kB_psram, size_psram);
    ESP_LOGI(TAG, "Prepare for speed test write from RAM, block = 10kB");
    sdcard_test_write_speed_(file_testing_ram, data_10kB_ram, size_ram);

    //Clear data
    memset((void*)data_10kB_psram, 0x00, size_psram);
    memset((void*)data_10kB_ram, 0x00, size_ram);

    //Test read
    ESP_LOGI(TAG, "Prepare for speed test read to RAM, block = 10kB");
    sdcard_test_read_speed_(file_testing_psram, data_10kB_psram, size_psram);
    ESP_LOGI(TAG, "Prepare for speed test read to PSRAM, block = 10kB");
    sdcard_test_read_speed_(file_testing_ram, data_10kB_ram, size_ram);

    //Free memory
    free(data_10kB_psram);
    free(data_10kB_ram);
}

void WriteOnceNextLine()
{   
    char* n = "\n";
    WriteOnce(n);
}

void WriteOnce(char* msg)
{
    FILE *f = fopen(MOUNT_POINT"/l_once.txt", "a");
    if (f == NULL) {
        #ifdef SDCARD_ENABLE_ERROR
            ESP_LOGE(TAG, "Failed to open file for log_once");
        #endif
        return;
    }

    //Write
    fprintf(f, "%s", msg);
    fclose(f);
}

void sdcard_clear_log(uint8_t is_once)
{
    FILE *f = fopen( is_once ? MOUNT_POINT"/l_once.txt" : MOUNT_POINT"/log.txt", "w");
    if (f == NULL) {
        #ifdef SDCARD_ENABLE_ERROR
            ESP_LOGE(TAG, "Failed to open file for log_once");
        #endif
        return;
    }
    fclose(f);
}

void WriteContinuous(char* msg, uint8_t enable, uint8_t flush)
{
    static uint8_t enabled = false;
    static FILE *f;

    //Control file
    if(enable == true && enabled == false)
    {
        f = fopen(MOUNT_POINT"/log.txt", "a");
        if (f == NULL) 
        {
            #ifdef SDCARD_ENABLE_ERROR
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