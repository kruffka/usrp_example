#ifndef COMMON_LIB_H
#define COMMON_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define MAX_WRITE_THREAD_PACKAGE 20
#define MAX_WRITE_THREAD_BUFFER_SIZE 8
#define MAX_CAPTURE_THREAD_PACKAGE 80

typedef long hw_timestamp;
typedef volatile long hw_vtimestamp;

/*!\brief structure holds the parameters to configure USRP devices*/
typedef struct hw_device_t hw_device;

// #define USRP_GAIN_OFFSET (56.0)  // 86 calibrated for USRP B210 @ 2.6 GHz to

typedef enum {
    max_gain = 0,
    med_gain,
    byp_gain
} rx_gain_t;

typedef enum {
    duplex_mode_TDD = 1,
    duplex_mode_FDD = 0
} duplex_mode_t;

/** @addtogroup _GENERIC_PHY_RF_INTERFACE_
 * @{
 */
/*!\brief RF device types
 */
typedef enum {
    MIN_RF_DEV_TYPE = 0,
  /*!\brief device is USRP B200/B210*/
    USRP_B200_DEV,
  /*!\brief device is USRP X300/X310*/
    USRP_X300_DEV,
  /*!\brief device is USRP N300/N310*/
    USRP_N300_DEV,
  /*!\brief device is USRP X400/X410*/
    USRP_X400_DEV,
  /*!\brief device is BLADE RF*/
    BLADERF_DEV,
  /*!\brief device is LMSSDR (SoDeRa)*/
    LMSSDR_DEV,
  /*!\brief device is Iris */
    IRIS_DEV,
  /*!\brief device is NONE*/
    NONE_DEV,
  /*!\brief device is FAPI */
    FAPI_DEV,
    RFSIMULATOR,
    MAX_RF_DEV_TYPE
} dev_type_t;

/*!\brief transport protocol types
 */
typedef enum {
    MIN_TRANSP_TYPE = 0,
  /*!\brief transport protocol ETHERNET */
    ETHERNET_TP,
  /*!\brief no transport protocol*/
    NONE_TP,
    MAX_TRANSP_TYPE
} transport_type_t;

/*!\brief  hw device host type */
typedef enum {
    MIN_HOST_TYPE = 0,
  /*!\brief device functions within a RAU */
    RAU_HOST,
  /*!\brief device functions within a RRU */
    RRU_HOST,
    MAX_HOST_TYPE
} host_type_t;

/*! \brief RF Gain clibration */
typedef struct {
  //! Frequency for which RX chain was calibrated
    double freq;
  //! Offset to be applied to RX gain
    double offset;
} rx_gain_calib_table_t;

/*! \brief Clock source types */
typedef enum {
  //! this means the paramter has not been set
    unset = -1,
  //! This tells the underlying hardware to use the internal reference
    internal = 0,
  //! This tells the underlying hardware to use the external reference
    external = 1,
  //! This tells the underlying hardware to use the gpsdo reference
    gpsdo = 2
} clock_source_t;

/*! \brief Radio Tx burst flags */
typedef enum {
    Invalid = 0,
    MiddleOfBurst,
    StartOfBurst,
    EndOfBurst
} radio_tx_flag_t;

/*! \brief RF frontend parameters set by application */
typedef struct {
  //! Module ID for this configuration
    int Mod_id;
  //! device log level
    int log_level;
  //! duplexing mode
    duplex_mode_t duplex_mode;
  //! number of downlink resource blocks
    int num_rb_dl;
  //! number of samples per frame
    unsigned int samples_per_frame;
  //! the sample rate for both transmit and receive.
    double sample_rate;
  //! flag to indicate that the device is doing mmapped DMA transfers
    int mmapped_dma;
  //! offset in samples between TX and RX paths
    int tx_sample_advance;
  //! samples per packet on the fronthaul interface
    int samples_per_packet;
  //! number of RX channels (=RX antennas)
    int rx_num_channels;
  //! number of TX channels (=TX antennas)
    int tx_num_channels;
  //! rx daughter card
    char *rx_subdev;
  //! tx daughter card
    char *tx_subdev;
  //! \brief RX base addresses for mmapped_dma
    int32_t *rxbase[4];
  //! \brief RX buffer size for direct access
    int rxsize;
  //! \brief TX base addresses for mmapped_dma or direct access
    int32_t *txbase[4];
  //! \brief Center frequency in Hz for RX.
  //! index: [0..rx_num_channels[
    double rx_freq[4];
  //! \brief Center frequency in Hz for TX.
  //! index: [0..rx_num_channels[ !!! see lte-ue.c:427 FIXME iterates over rx_num_channels
    double tx_freq[4];
    double tune_offset;
  //! \brief memory
  //! \brief Pointer to Calibration table for RX gains
    rx_gain_calib_table_t *rx_gain_calib_table;
  //! mode for rxgain (ExpressMIMO2)
    rx_gain_t rxg_mode[4];
  //! \brief Gain for RX in dB.
  //! index: [0..rx_num_channels]
    double rx_gain[4];
  //! \brief Gain offset (for calibration) in dB
  //! index: [0..rx_num_channels]
    double rx_gain_offset[4];
  //! gain for TX in dB
    double tx_gain[4];
  //! RX bandwidth in Hz
    double rx_bw;
  //! TX bandwidth in Hz
    double tx_bw;
  //! clock source
    clock_source_t clock_source;
  //! time_source
    clock_source_t time_source;
  //! Manual SDR IP address
    char *sdr_addrs;
  //! Auto calibration flag
    int autocal[4];
  //! rf devices work with x bits iqs when have its own iq format
  //! the two following parameters are used to convert iqs
    int iq_txshift;
    int iq_rxrescale;
  //! Configuration file for LMS7002M
    char *configFilename;
  //! remote IP/MAC addr for Ethernet interface
    char *remote_addr;
  //! remote port number for Ethernet interface
    unsigned int remote_port;
  //! local IP/MAC addr for Ethernet interface (eNB/BBU, UE)
    char *my_addr;
  //! local port number for Ethernet interface (eNB/BBU, UE)
    unsigned int my_port;
  //! number of samples per tti
    unsigned int samples_per_tti;
  //! the sample rate for receive.
    double rx_sample_rate;
  //! the sample rate for transmit.
    double tx_sample_rate;
  //! check for threequarter sampling rate
    int8_t threequarter_fs;
  //! NR band number
    int nr_band;
  //! NR scs for raster
    int nr_scs_for_raster;
  //! Core IDs for RX FH
    int rxfh_cores[4];
  //! Core IDs for TX FH
    int txfh_cores[4];
} hw_config_t;

/*! \brief RF mapping */
typedef struct {
  //! card id
    int card;
  //! rf chain id
    int chain;
} hw_rf_map;

typedef struct {
    char *remote_addr;
  //! remote port number for Ethernet interface (control)
    uint16_t remote_portc;
  //! remote port number for Ethernet interface (user)
    uint16_t remote_portd;
  //! local IP/MAC addr for Ethernet interface (eNB/RAU, UE)
    char *my_addr;
  //! local port number (control) for Ethernet interface (eNB/RAU, UE)
    uint16_t my_portc;
  //! local port number (user) for Ethernet interface (eNB/RAU, UE)
    uint16_t my_portd;
  //! local Ethernet interface (eNB/RAU, UE)
    char *local_if_name;
  //! transport type preference  (RAW/UDP)
    uint8_t transp_preference;
  //! compression enable (0: No comp/ 1: A-LAW)
    uint8_t if_compress;
} eth_params_t;

typedef struct {
  //! Tx buffer for if device, keep one per subframe now to allow multithreading
    void *tx[10];
  //! Tx buffer (PRACH) for if device
    void *tx_prach;
  //! Rx buffer for if device
    void *rx;
} if_buffer_t;

typedef struct {
    hw_timestamp timestamp;
    void *buff[MAX_WRITE_THREAD_BUFFER_SIZE]; // buffer to be write;
    int nsamps;
    int cc;
    signed char first_packet;
    signed char last_packet;
    int flags_msb;
} hw_write_package_t;

typedef struct {
    hw_write_package_t write_package[MAX_WRITE_THREAD_PACKAGE];
    int start;
    int end;
  /// \internal This variable is protected by \ref mutex_write
    int count_write;
  /// pthread struct for trx write thread
    pthread_t pthread_write;
  /// pthread attributes for trx write thread
    pthread_attr_t attr_write;
  /// condition varible for trx write thread
    pthread_cond_t cond_write;
  /// mutex for trx write thread
    pthread_mutex_t mutex_write;
  /// to inform the thread to exit
    bool write_thread_exit;
} hw_thread_t;

typedef struct {
    hw_write_package_t write_package[MAX_CAPTURE_THREAD_PACKAGE];
    int is_rxtx;
    int start;
    int end;
  /// \internal This variable is protected by \ref mutex_write
    int count_write;
  /// pthread struct for trx write thread
    pthread_t pthread_write;
  /// pthread attributes for trx write thread
    pthread_attr_t attr_write;
  /// condition varible for trx write thread
    pthread_cond_t cond_write;
  /// mutex for trx write thread
    pthread_mutex_t mutex_write;
  /// to inform the thread to exit
    bool rxtx_capture_exit;
    int write_chunk;
    int slots_per_frame;
    char iq_record_path[256];
} hw_capture_thread_t;

typedef struct nr_phy_time_s {
    uint32_t frame;
    uint32_t slot;
    uint32_t tti;
    uint32_t slots_per_frame;
} nr_phy_time_t;

/*!\brief structure holds the parameters to configure USRP devices */
struct hw_device_t {
  /*!tx write thread*/
    hw_thread_t write_thread;

      /*!rx tx capture iq thread*/
    hw_capture_thread_t rx_capture_thread;
    hw_capture_thread_t tx_capture_thread;

  /*!brief Module ID of this device */
    int Mod_id;

  /*!brief Component Carrier ID of this device */
    int CC_id;

  /*!brief Type of this device */
    dev_type_t type;

  /*!brief Transport protocol type that the device supports (in case I/Q samples
     * need to be transported) */
    transport_type_t transp_type;

  /*!brief Type of the device's host (RAU/RRU) */
    host_type_t host_type;

  /* !brief RF frontend parameters set by application */
    hw_config_t *hw_config;

  /* !brief Indicates if device already initialized */
    int is_init;

  /*!brief Can be used by driver to hold internal structure*/
    void *priv;

  /* Functions API, which are called by the application*/

  /*! \brief Called to start the transceiver. Return 0 if OK, < 0 if error
                      @param device pointer to the device structure specific to the RF hardware
                     target
                  */
    int (*trx_start_func)(hw_device *device);

  /*! \brief Called to configure the device
                       @param device pointer to the device structure specific to the RF hardware
                     target
                   */

    int (*trx_config_func)(hw_device *device, hw_config_t *hw_config);

  /*! \brief Called to send a request message between RAU-RRU on control port
                      @param device pointer to the device structure specific to the RF hardware
                     target
                      @param msg pointer to the message structure passed between RAU-RRU
                      @param msg_len length of the message
                  */
    int (*trx_ctlsend_func)(hw_device *device, void *msg, ssize_t msg_len);

  /*! \brief Called to receive a reply  message between RAU-RRU on control port
                      @param device pointer to the device structure specific to the RF hardware
                     target
                      @param msg pointer to the message structure passed between RAU-RRU
                      @param msg_len length of the message
                  */
    int (*trx_ctlrecv_func)(hw_device *device, void *msg, ssize_t msg_len);

  /*! \brief Called to send samples to the RF target
                      @param device pointer to the device structure specific to the RF hardware
                     target
                      @param timestamp The timestamp at whicch the first sample MUST be sent
                      @param buff Buffer which holds the samples (2 dimensional)
                      @param nsamps number of samples to be sent
                      @param number of antennas
                      @param flags flags must be set to TRUE if timestamp parameter needs to be
                     applied
                  */
    int (*trx_write_func)(hw_device *device,
                          hw_timestamp timestamp,
                          void **buff,
                          int nsamps,
                          int antenna_id,
                          int flags);

  /*! \brief Receive samples from hardware.
     * Read \ref nsamps samples from each channel to buffers. buff[0] is the array
     * for the first channel. *ptimestamp is the time at which the first sample
     * was received.
     * \param device the hardware to use
     * \param[out] ptimestamp the time at which the first sample was received.
     * \param[out] buff An array of pointers to buffers for received samples. The
     * buffers must be large enough to hold the number of samples \ref nsamps.
     * \param nsamps Number of samples. One sample is 2 byte I + 2 byte Q => 4
     * byte. \param num_antennas number of antennas from which to receive samples
     * \returns the number of sample read
     */
    int (*trx_read_func)(hw_device *device,
                         hw_timestamp *ptimestamp,
                         void **buff,
                         int nsamps,
                         int num_antennas);

    int (*trx_get_stats_func)();

  /*! \brief Reset device statistics
     * \returns 0 in success
     */
    int (*trx_reset_stats_func)();

  /*! \brief Terminate operation of the transceiver -- free all associated
     * resources \param device the hardware to use
     */
    void (*trx_end_func)(hw_device *);

  /*! \brief Stop operation of the transceiver
     */
    void (*trx_stop_func)(hw_device *device);

  /* Functions API related to UE*/

  /*! \brief Set RX feaquencies
     * \param device the hardware to use
     * \param hw_config RF frontend parameters set by application
     * \param exmimo_dump_config  dump EXMIMO configuration
     * \returns 0 in success
     */
    int (*trx_set_freq_func)(hw_device *device, hw_config_t *hw_config);

  /*! \brief Set gains
     * \param device the hardware to use
     * \param hw_config RF frontend parameters set by application
     * \returns 0 in success
     */
    int (*trx_set_gains_func)(hw_device *device, hw_config_t *hw_config);

  /*! \brief RRU Configuration callback
     * \param idx RU index
     * \param arg pointer to capabilities or configuration
     */
    void (*configure_rru)(int idx, void *arg);

  /*! \brief Pointer to generic RRU private information
     */

    void *thirdparty_priv;

  /*! \brief Callback for Third-party RRU Initialization routine
                     \param device the hardware configuration to use
                   */
    int (*thirdparty_init)(hw_device *device);

  /*! \brief Callback for Third-party RRU Cleanup routine
                     \param device the hardware configuration to use
                   */
    int (*thirdparty_cleanup)(hw_device *device);

  /*! \brief Callback for Third-party start streaming routine
                     \param device the hardware configuration to use
                   */
    int (*thirdparty_startstreaming)(hw_device *device);

  /*! \brief RRU Configuration callback
     * \param idx RU index
     * \param arg pointer to capabilities or configuration
     */
    int (*trx_write_init)(hw_device *device);

  /*! \brief RRU Configuration callback
     * \param idx RU index
     * \param arg pointer to capabilities or configuration
     */
    int (*trx_capture_init)(hw_device *device);

  /* \brief Get internal parameter
     * \param id parameter to get
     * \return a pointer to the parameter
     */
    void *(*get_internal_parameter)(char *id);
  /// function pointer to start a thread of tx write for USRP.
    int (*start_write_thread)(hw_device *device);
};

#define UE_MAGICDL 0xA5A5A5A5A5A5A5A5 // UE DL FDD record
#define UE_MAGICUL 0x5A5A5A5A5A5A5A5A // UE UL FDD record

#define OPTION_LZ4 0x00000001 // LZ4 compression (option_value is set to compressed size)

typedef struct {
    uint64_t magic; // Magic value (see defines above)
    uint32_t size;  // Number of samples per antenna to follow this header
    uint32_t nbAnt; // Total number of antennas following this header
  // Samples per antenna follow this header,
  // i.e. nbAnt = 2 => this header+samples_antenna_0+samples_antenna_1
  // data following this header in bytes is nbAnt*size*sizeof(sample_t)
    uint64_t timestamp;    // Timestamp value of first sample
    uint32_t option_value; // Option value
    uint32_t option_flag;  // Option flag
} samplesBlockHeader_t;

#endif // COMMON_LIB_H
