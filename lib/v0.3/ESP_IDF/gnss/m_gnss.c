#include "m_gnss.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "gnss";

#define GNSS_GLL        0
#define GNSS_RMC        1
#define GNSS_GGA        2
#define GNSS_GSA        3
#define GNSS_GSA_4_11   4
#define GNSS_GSV7       5
#define GNSS_GSV11      6
#define GNSS_GSV15      7
#define GNSS_GSV19      8
#define GNSS_RMC_4_1    9
#define GNSS_ST_MIN     GNSS_GLL
#define GNSS_ST_MAX     GNSS_RMC_4_1

static char* GNSS_SENTENCE_PARAMS[10] = {
    //0 - GNSS_GLL
    "dcdcscC",
    //1 - GNSS_RMC
    "scdcdcffsDCC",
    //2 - GNSS_GGA
    "sdcdciiffsfsIS",
    //3 - GNSS_GSA
    "ciIIIIIIIIIIIIfff",
    //4 - GNSS_GSA_4_11
    "ciIIIIIIIIIIIIfffS",
    //5 - GNSS_GSV7
    "iiiiiiI",
    //6 - GNSS_GSV11
    "iiiiiiIiiiI",
    //7 - GNSS_GSV15
    "iiiiiiIiiiIiiiI",
    //8 - GNSS_GSV19
    "iiiiiiIiiiIiiiIiiiI",
    //9 - GNSS_RMC_4_1
    "scdcdcffsDCCC"};

static char sentence_byte[1024] = {0};
static int s_indx = 0;
m_gnss_locals_t m_gnss_locals = {0};

static esp_err_t gnss_read_sentence();
static esp_err_t gnss_parse_sentence();
static esp_err_t gnss_read_from_bytes(int* start_indx);
static esp_err_t gnss_parse_talker();
static void gnss_create_arg_list();

static esp_err_t gnss_parse_data(int sentence_type);
static bool gnss_parse_gll();
static bool gnss_parse_rmc();
static bool gnss_parse_gga();
static bool gnss_parse_gsv();
static bool gnss_parse_gsa();

static int gnss_parse_degrees(char* nmea_data);
static float gnss_parse_float(char* nmea_data);
static int gnss_parse_int(char* nmea_data);

static void gnss_read_degrees(int type, int indx, char neg);
static void gnss_read_int_degrees(int type, int indx, char neg);
static void gnss_update_timestamp_utc(int t_indx, int d_indx);

static bool uart_inited = false;

esp_err_t gnss_init(int baudrate)
{
	uart_config_t uart_config = {
		.baud_rate = baudrate,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.source_clk = UART_SCLK_DEFAULT,
	};

	// Configure UART parameters
	uart_driver_install(UART_NUM_2, 1024 * 10, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_2, &uart_config);

	//GPS UART
	uart_set_pin(UART_NUM_2, GPIO_NUM_17, GPIO_NUM_18, 0, 0);

    //Reset all
    memset(&m_gnss_locals, 0x00, sizeof(m_gnss_locals));

    uart_inited = true;

    ESP_LOGI(TAG, "Successfull initialized");
    return ESP_OK;
}

esp_err_t gnss_deinit()
{
    if(uart_inited)
        uart_driver_delete(UART_NUM_2);
    uart_inited = false;
    return ESP_OK;
}


esp_err_t gnss_update(bool* result)
{
        //Returns true if new data was processed, and false if nothing new was received.
        if (gnss_parse_sentence() != ESP_OK)
            return ESP_FAIL;

        if (GNSS_ENABLE_INFO)
            ESP_LOGI(TAG, "Recievied gnnss, type=%s, data=%s", m_gnss_locals._data_type, m_gnss_locals._raw_sentence);
        
        //data_type, args = sentence
        if (strlen(m_gnss_locals._data_type) != 6)
            return ESP_FAIL;

        //Split datatype
        gnss_parse_talker();

        //Check for all currently known GNSS talkers
        //GA - Galileo
        //GB - BeiDou Systems
        //GI - NavIC
        //GL - GLONASS
        //GP - GPS
        //GQ - QZSS
        //GN - GNSS / More than one of the above
        char check_[7][2] = {"GA", "GB","GI", "GL" ,"GP", "GQ", "GN"};
        bool check = false;
        for(uint8_t i=0; i<7; i++)
        {            
            if( memcmp(m_gnss_locals._talker, check_[i], 2) == 0)
                    check = true;
        }

        if(check == false)
        {
            //It's not a known GNSS source of data
            //Assume it's a valid packet anyway
            return ESP_OK;
        }

        *result = true;
        gnss_create_arg_list();

        //Geographic position - Latitude/Longitude
        if (memcmp(m_gnss_locals._sentence_type, "GLL", 3) == 0)  
            *result = gnss_parse_gll();
        //Minimum location info
        else if (memcmp(m_gnss_locals._sentence_type, "RMS", 3) == 0)  
            *result = gnss_parse_rmc();
        else if (memcmp(m_gnss_locals._sentence_type, "GGA", 3) == 0)  
            *result = gnss_parse_gga();
        else if (memcmp(m_gnss_locals._sentence_type, "GSV", 3) == 0)  
            *result = gnss_parse_gsv();
        else if (memcmp(m_gnss_locals._sentence_type, "GSA", 3) == 0)  
            *result = gnss_parse_gsa();
    return ESP_OK;
}




esp_err_t gnss_parse_sentence()
{
    //sentence is a valid NMEA with a valid checksum
    if (gnss_read_sentence() != ESP_OK)
        return ESP_FAIL;

    int len_sentence = strlen(m_gnss_locals._raw_sentence);

    //Remove checksum once validated.
    memset(&m_gnss_locals._raw_sentence[len_sentence-1-2], 0, 3);

    //Parse out the type of sentence (first string after $ up to comma)
    //and then grab the rest as data within the sentence.
    char* delimiter = strchr(m_gnss_locals._raw_sentence, ',');

    //Invalid sentence, no comma after data type.
    if (delimiter == NULL)
        return ESP_FAIL;  
    
    //Copy datatype
    int del_index = (uint32_t)delimiter - (uint32_t)m_gnss_locals._raw_sentence;
    memcpy(m_gnss_locals._data_type, m_gnss_locals._raw_sentence, del_index);
    m_gnss_locals._data_type[del_index] = 0;

    //String shift
    char buff[256] = {0};
    strcpy(buff, &m_gnss_locals._raw_sentence[del_index+1]);
    strcpy(m_gnss_locals._raw_sentence, buff);

    return ESP_OK;
}
    

esp_err_t gnss_read_sentence()
{
    //Parse any NMEA sentence that is available.

    int indx = 0;
    esp_err_t ret = gnss_read_from_bytes(&indx);
    if(ret != ESP_OK)
        return ret;

    //Sentence
    char* sentence = &sentence_byte[indx];

    //Strip
    uint16_t indx_s = 0;
    uint16_t indx_e = strlen(sentence) - 1;
    while(sentence[indx_s] && (sentence[indx_s] == ' ' || sentence[indx_s] == '\n' || sentence[indx_s]=='\r' || sentence[indx_s]=='\t')) 
    {
        indx_s++;
    }
    while(indx_e > 0 && (sentence[indx_e] == ' ' || sentence[indx_e] == '\n' || sentence[indx_e]=='\r' || sentence[indx_e]=='\t')) 
    {
        sentence[indx_e] = 0; 
        indx_e--;
    };


    //Look for a checksum and validate it if present.
    char* new_sentence = &sentence[indx_s];
    int len_sentence = strlen(new_sentence);

    if (len_sentence > 7 && new_sentence[len_sentence-3] == '*')
    {
        //Get included checksum, then calculate it and compare.
        int expected = strtol(&new_sentence[len_sentence-2], NULL, 16);
        int actual = 0;
        for (uint16_t i=1; i< len_sentence - 3; i++)
            actual ^= new_sentence[i];

        //Failed to validate checksum.
        if((actual&0xFF) != expected)
            return ESP_FAIL;

        //copy the raw new_sentence
        strcpy(m_gnss_locals._raw_sentence, new_sentence);
        strcpy(m_gnss_locals._raw_sentence_print, m_gnss_locals._raw_sentence);
        
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t gnss_read_from_bytes(int* start_indx)
{   
    static bool start = false;
    static int start_index = 0;

    //Get avaliable byte count
	size_t bytesRead;
	uart_get_buffered_data_len(UART_NUM_2, &bytesRead);

    //Copy byte from uart buffer to internal buffer
    char c;
    while(bytesRead)
    {   
        //Read byte
        uart_read_bytes(UART_NUM_2, &c, 1, 10 / portTICK_PERIOD_MS);

        switch(c)
        {   
            //Sentence begin
            case '$':
            {
                //Reset indx
                s_indx = 0;
                start = true;
                start_index = s_indx;
                sentence_byte[s_indx++] = c;
            }break;
            //Ordinary characters
            default: 
            {   
                //Drop fragmented messages
                if (s_indx > sizeof(sentence_byte) - 5)
                {   
                    start = false;
                    start_index = 0;
                    s_indx = 0;
                }
                else
                    sentence_byte[s_indx++] = c;
            }break;
            //Sentence end
            case '\n':
            {
                if(start == true)
                {
                    start = false;
                    *start_indx = start_index;
                    sentence_byte[s_indx] = 0;
                    return ESP_OK;
                }
            }break;
        }
        //Get avaliable byte count
        uart_get_buffered_data_len(UART_NUM_2, &bytesRead);
    }

    return ESP_FAIL;
}






esp_err_t gnss_parse_talker()
{
    //Split the data_type into talker and sentence_type
    memcpy(m_gnss_locals._talker, &m_gnss_locals._data_type[1], 2);
    m_gnss_locals._talker[2] = 0;

    memcpy(m_gnss_locals._sentence_type, &m_gnss_locals._data_type[3], 3);
    m_gnss_locals._sentence_type[3] = 0; 

    return ESP_OK;
}

bool gnss_parse_gll()
{
    //GLL - Geographic Position - Latitude/Longitude

    //Unexpected number of params.
    if (m_gnss_locals.arg_len != 7)
        return false;

    //Params didn't parse
    esp_err_t ret = gnss_parse_data(GNSS_GLL);
    if (ret != ESP_OK)
        return false  ;

    //Latitude
    gnss_read_degrees(0, 0, 'S');
    gnss_read_int_degrees(0, 0, 'S');

    //Longitude
    gnss_read_degrees(1, 2, 'W');
    gnss_read_int_degrees(1, 2, 'W');

    //UTC time of position
    gnss_update_timestamp_utc(4 , -1);

    //Status Valid(A) or Invalid(V)
    m_gnss_locals.isactivedata = *((char*)m_gnss_locals.param_list[5]);

    return ESP_OK;
}

bool gnss_parse_rmc()
{
    //RMC - Recommended Minimum Navigation Information

    //Unexpected number of params.
    if (m_gnss_locals.arg_len != 12 && m_gnss_locals.arg_len != 13)
        return false;

    esp_err_t ret = ESP_FAIL;
    if(m_gnss_locals.arg_len == 12)
        ret = gnss_parse_data(GNSS_RMC);
    if(m_gnss_locals.arg_len == 13)
        ret = gnss_parse_data(GNSS_RMC_4_1);

    //Params didn't par
    if(ret != ESP_OK)
    {
        m_gnss_locals.fix_quality = 0;
        return false;
    }

    //UTC time of position and date
    gnss_update_timestamp_utc(0 , 8);

    //Status Valid(A) or Invalid(V)
    m_gnss_locals.isactivedata = *((char*)m_gnss_locals.param_list[1]);

    if(tolower(m_gnss_locals.isactivedata ) == 'a')
    {
        if (m_gnss_locals.fix_quality == 0)
            m_gnss_locals.fix_quality = 1;
    }
    else
        m_gnss_locals.fix_quality = 0;

    //Latitude
    gnss_read_degrees(0, 2, 'S');
    gnss_read_int_degrees(0, 2, 'S');

    //Longitude
    gnss_read_degrees(1, 4, 'W');
    gnss_read_int_degrees(1, 4, 'W');

    //Speed over ground, knots
    m_gnss_locals.speed_knots = *((float*)m_gnss_locals.param_list[6]);

    //Track made good, degrees true
    m_gnss_locals.track_angle_deg = *((float*)m_gnss_locals.param_list[7]);

    return true;
}

bool gnss_parse_gga()
{
//GGA - Global Positioning System Fix Data

    //Unexpected number of params.
    if (m_gnss_locals.arg_len != 14)
        return false;

    esp_err_t ret = gnss_parse_data(GNSS_GGA);

    //Params didn't par
    if (ret != ESP_OK)
    {
        m_gnss_locals.fix_quality = 0;
        return false;
    }

    //UTC time of position
    gnss_update_timestamp_utc(0, -1);

    //Latitude
    gnss_read_degrees(0, 1, 'S');
    gnss_read_int_degrees(0, 1, 'S');

    //Longitude
    gnss_read_degrees(1, 3, 'W');
    gnss_read_int_degrees(1, 3, 'W');


    //GPS quality indicator
    m_gnss_locals.fix_quality = *((int*)m_gnss_locals.param_list[5]);

    //Number of satellites in use, 0 - 12
    m_gnss_locals.satellites = *((int*)m_gnss_locals.param_list[6]);

    //Horizontal dilution of precision
    m_gnss_locals.horizontal_dilution = *((float*)m_gnss_locals.param_list[7]);

    //Antenna altitude relative to mean sea level
    m_gnss_locals.altitude_m = *((float*)m_gnss_locals.param_list[8]);
    //data[9] - antenna altitude unit, always 'M' ???

    //Geoidal separation relative to WGS 84
    m_gnss_locals.height_geoid = *((float*)m_gnss_locals.param_list[10]);
    //data[11] - geoidal separation unit, always 'M' ???

    //data[12] - Age of differential GPS data, can be null
    //data[13] - Differential reference station ID, can be null

    return true;
}


bool gnss_parse_gsv()
{
    //GSV - Satellites in view
    //Unexpected number of params.
    if (m_gnss_locals.arg_len != 7 && m_gnss_locals.arg_len != 11 && m_gnss_locals.arg_len != 15 && m_gnss_locals.arg_len != 19)
        return false;


    esp_err_t ret = ESP_FAIL;
    if(m_gnss_locals.arg_len == 7)
        ret = gnss_parse_data(GNSS_GSV7);
    if(m_gnss_locals.arg_len == 11)
        ret = gnss_parse_data(GNSS_GSV11);
    if(m_gnss_locals.arg_len == 15)
        ret = gnss_parse_data(GNSS_GSV15);
    if(m_gnss_locals.arg_len == 19)
        ret = gnss_parse_data(GNSS_GSV19);
    //Params didn't par
    if (ret != ESP_OK)
    {
        m_gnss_locals.fix_quality = 0;
        return false;
    }

    //Number of messages
    m_gnss_locals.total_mess_num = *((int*)m_gnss_locals.param_list[0]);

    //Message number
    m_gnss_locals.mess_num = *((int*)m_gnss_locals.param_list[1]);
    //Number of satellites in view
    m_gnss_locals.satellites = *((int*)m_gnss_locals.param_list[2]);

    // sat_tup = data[3:]
    // satlist = []
    // timestamp = time.monotonic()
    // for i in range(len(sat_tup) // 4):
    //     j = i * 4
    //     value = (
    //         //Satellite number
    //         "{}{}".format(talker, sat_tup[0 + j]),
    //         //Elevation in degrees
    //         sat_tup[1 + j],
    //         //Azimuth in degrees
    //         sat_tup[2 + j],
    //         //signal-to-noise ratio in dB
    //         sat_tup[3 + j],
    //         //Timestamp
    //         timestamp,
    //     )
    //     satlist.append(value)

    // if self._sats is INT32_MIN:
    //     self._sats = []
    // for value in satlist:
    //     self._sats.append(value)

    // if m_gnss_locals.mess_num == m_gnss_locals.total_mess_num:
    //     //Last part of GSV message
    //     if len(self._sats) == m_gnss_locals.satellites:
    //         //Transfer received satellites to self.sats
    //         if self.sats is INT32_MIN:
    //             self.sats = {}
    //         else:
    //             //Remove all satellites which haven't
    //             //been seen for 30 seconds
    //             timestamp = time.monotonic()
    //             old = []
    //             for sat_id, sat_data in self.sats.items():
    //                 if (timestamp - sat_data[4]) > 30:
    //                     old.append(sat_id)
    //             for i in old:
    //                 self.sats.pop(i)
    //         for sat in self._sats:
    //             self.sats[sat[0]] = sat
    //     self._sats.clear()

    m_gnss_locals.satellites_prev = m_gnss_locals.satellites;

    return true;
}


bool gnss_parse_gsa()
{
    //GSA - GPS DOP and active satellites


    //Unexpected number of params.
    if (m_gnss_locals.arg_len != 17 && m_gnss_locals.arg_len != 18)
        return false;


    esp_err_t ret = ESP_FAIL;
    if(m_gnss_locals.arg_len == 17)
        ret = gnss_parse_data(GNSS_GSA);
    if(m_gnss_locals.arg_len == 18)
        ret = gnss_parse_data(GNSS_GSA_4_11);

    //Params didn't par
    if (ret != ESP_OK)
    {
        m_gnss_locals.fix_quality_3d = 0;
        return false;
    }

    //Selection mode: 'M' - manual, 'A' - automatic
    m_gnss_locals.sel_mode = *((char*)m_gnss_locals.param_list[0]);

    //Mode: 1 - no fix, 2 - 2D fix, 3 - 3D fix
    m_gnss_locals.fix_quality_3d = *((int*)m_gnss_locals.param_list[1]);

    //PDOP, dilution of precision
    m_gnss_locals.pdop = *((float*)m_gnss_locals.param_list[14]);

    //HDOP, horizontal dilution of precision
    m_gnss_locals.hdop = *((float*)m_gnss_locals.param_list[15]);

    //VDOP, vertical dilution of precision
    m_gnss_locals.vdop = *((float*)m_gnss_locals.param_list[16]);

    //data[17] - System ID

    return true;
}

esp_err_t gnss_parse_data(int sentence_type)
{
    //Parse sentence data for the specified sentence type to list of parameters in the correct format

    //The sentence_type is unknown
    if (GNSS_ST_MAX < sentence_type || sentence_type < GNSS_ST_MIN)
        return ESP_FAIL;

    char* param_types = GNSS_SENTENCE_PARAMS[sentence_type];

    //The expected number does not match the number of data items
    if (strlen(param_types) != m_gnss_locals.arg_len)
        return ESP_FAIL;

    //Reset parameters
    memset(m_gnss_locals.param_list, 0, sizeof(m_gnss_locals.param_list));

    for(uint8_t i=0; i<20; i++)
    {       
        char pti = param_types[i];
        if(pti==0)
            break;

        int len_dti = 0;
        bool nothing = false;

        char* dti = m_gnss_locals.arg_list[i];
        if(dti[0] == 0)
            nothing = true;
        else
            len_dti = strlen(dti);

        //A single character
        if (pti == 'c')
        {
            if (len_dti != 1)
                return ESP_FAIL;

            m_gnss_locals.param_list_char[i] = *dti;
            m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_char[i];
        }
        //A single character or Nothing
        else if (pti == 'C')
        {
            if (nothing)
                m_gnss_locals.param_list[i] = NULL;
            else if (len_dti != 1)
                return ESP_FAIL;
            else
            {
                m_gnss_locals.param_list_char[i] = *dti;
                m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_char[i];
            }
        }
        //A number parseable as degrees
        else if (pti == 'd')
        {
            m_gnss_locals.param_list_int[i] = gnss_parse_degrees(dti);
            m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_int[i];
        }
        //A number parseable as degrees or Nothing
        else if (pti == 'D')
        {
            if (nothing)
                m_gnss_locals.param_list[i] = NULL;
            else
            {
            m_gnss_locals.param_list_int[i] = gnss_parse_degrees(dti);
            m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_int[i];
            }
        }
        //A floating point number
        else if (pti =='f')
        {
            m_gnss_locals.param_list_float[i] = gnss_parse_float(dti);
            m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_float[i];
        }
        //An integer
        else if (pti == 'i')
        {
            m_gnss_locals.param_list_int[i] = gnss_parse_int(dti);
            m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_int[i];
        }
        //An integer or Nothing
        else if (pti == 'I')
        {
            if (nothing)
                m_gnss_locals.param_list[i] = NULL;
            else
            {
                m_gnss_locals.param_list_int[i] = gnss_parse_int(dti);
                m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_int[i];
            }
        }
        //A String
        else if (pti == 's')
        {
            memcpy(m_gnss_locals.param_list_string[i], dti, 25);
            m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_string[i][0];
        }
        //A string or Nothing
        else if (pti ==  'S')
        {
            if (nothing)
                m_gnss_locals.param_list[i] = NULL;
            else
            {
                memcpy(m_gnss_locals.param_list_string[i], dti, 25);
                m_gnss_locals.param_list[i] = (uint32_t*)&m_gnss_locals.param_list_string[i][0];
            }
        }
        else
            return ESP_FAIL;
    }
    return ESP_OK; 
}

//Internal helper parsing functions.
//These handle input that might be INT32_MIN or null and return INT32_MIN instead of
//throwing errors.
int gnss_parse_degrees(char* nmea_data)
{
    //Parse a NMEA lat/long data pair 'dddmm.mmmm' into a pure degrees value.
    //Where ddd is the degrees, mm.mmmm is the minutes.
    if(nmea_data == 0)
        return 0;
    if(strlen(nmea_data) < 3)
        return 0;

    //To avoid losing precision handle degrees and minutes separately
    //Return the final value as an integer. Further functions can parse
    //this into a float or separate parts to retain the precision
    char *raw = strtok (nmea_data,".");

    int degrees = (atoi(raw)/100)*1000000;          //the ddd
    float minutes = (float)(atoi(raw) % 100);       //the mm.
    raw = strtok (NULL,".");

    uint8_t i=0;
    for( ;i<4;i++)
    {
        if(raw[i] == 0)
            raw[i] = '0';
    }
    raw[4] = 0;
    minutes += (float)atoi(raw) / 10000.0;
    minutes = minutes / 60.0 * 1000000.0;
    return degrees + (int)minutes;
}

float gnss_parse_float(char* nmea_data)
{
    if(nmea_data == 0)
        return 0;
    if(strlen(nmea_data) == 0)
        return 0;
    
    return atof(nmea_data);
}

int gnss_parse_int(char* nmea_data)
{
    if(nmea_data == 0)
        return 0;
    if(strlen(nmea_data) == 0)
        return 0;

    return atoi(nmea_data);
}


void gnss_read_degrees(int type, int indx, char neg)
{
    //This function loses precision with float32

    int x = *((int*)m_gnss_locals.param_list[indx]);
    float x_f = ((float)x) / 1000000.0;

    char *ch = (char*)m_gnss_locals.param_list[indx+1];
    if(*ch == neg || *ch == tolower(neg))
        x_f *= -1.0;

    if(type == 0)
        m_gnss_locals.latitude = x_f;
    if(type == 1)
        m_gnss_locals.longitude = x_f;
}


void gnss_read_int_degrees(int type, int indx, char neg)
{
    int x = *((int*)m_gnss_locals.param_list[indx]);
    
    int deg = x / 1000000;
    int minutes = x % 1000000 / 10000;

    char *ch = (char*)m_gnss_locals.param_list[indx+1];
    if(*ch == neg || *ch == tolower(neg))
        deg *= -1.0;


    if(type == 0)
    {
        m_gnss_locals.latitude_degrees = deg;
        m_gnss_locals.latitude_minutes = minutes;
    }
    if(type == 1)
    {
        m_gnss_locals.longitude_degrees = deg;
        m_gnss_locals.longitude_minutes = minutes;
    }

}


void gnss_update_timestamp_utc(int t_indx, int d_indx)
{
    char buf[25] = {0};

    char *x_string = (char*)m_gnss_locals.param_list[t_indx];

    snprintf(buf, 3, "%s", &x_string[0]);
    int hours = atoi(buf);

    snprintf(buf, 3, "%s", &x_string[2]);
    int mins = atoi(buf);

    snprintf(buf, 3, "%s", &x_string[4]);
    int secs = atoi(buf);

    int day, month, year;
    if (d_indx == -1)
    {
        day = m_gnss_locals.timestamp_utc.tm_mday;
        month = m_gnss_locals.timestamp_utc.tm_mon;
        year = m_gnss_locals.timestamp_utc.tm_year;
    }
    else
    {
        char *y_string = (char*)m_gnss_locals.param_list[d_indx];

        snprintf(buf, 3, "%s", &y_string[0]);
        day = atoi(buf);

        snprintf(buf, 3, "%s", &y_string[2]);
        month = atoi(buf);

        snprintf(buf, 3, "%s", &y_string[4]);
        year = 2000 + atoi(buf);
    }

    m_gnss_locals.timestamp_utc.tm_mday = day;
    m_gnss_locals.timestamp_utc.tm_mon = month;
    m_gnss_locals.timestamp_utc.tm_year = year;

    m_gnss_locals.timestamp_utc.tm_min = mins;
    m_gnss_locals.timestamp_utc.tm_hour = hours;
    m_gnss_locals.timestamp_utc.tm_sec = secs;

}



void gnss_create_arg_list()
{
    //Split sentence
    char *arg = m_gnss_locals._raw_sentence;
    uint8_t i=0;
    for( ; i<20; i++)
    {
        //Find comma
        char* comma = strchr(arg,',');
        if(comma == NULL)
            break;

        //Replace ',' to '\0'
        *comma = 0;

        //Len_arg == 0 
        if(comma == arg)
        {   
            memset(m_gnss_locals.arg_list[i], 0, sizeof(m_gnss_locals.arg_list[i]));
            arg++;
            continue;
        }

        //Save and next
        sprintf(m_gnss_locals.arg_list[i], "%s", arg);
        arg = comma+1;
    }

    m_gnss_locals.arg_len = i+1;
}


esp_err_t gnss_send_command(uint8_t* command, uint8_t len)
{
    // Send a command string to the GPS.  If add_checksum is true (the
    // default) a NMEA checksum will automatically be computed and added.
    // Note you should NOT add the leading $ and trailing * to the command
    // as they will automatically be added!
    

    uart_write_bytes(UART_NUM_2, (const char *)"$", 1);
    uart_write_bytes(UART_NUM_2, command, len);
    int checksum = 0;
    for(uint16_t i=0;i<len;i++)
        checksum ^= command[i];

    char buf[10]={0};
    uint8_t buf_len = sprintf(buf, "*%02X\r\n", (uint8_t)checksum);
    uart_write_bytes(UART_NUM_2, buf, buf_len);

    return ESP_OK;
}