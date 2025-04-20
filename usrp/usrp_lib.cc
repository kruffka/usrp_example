// file usrp_lib.cpp
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#include <uhd/utils/thread.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/usrp/subdev_spec.hpp>
#include <uhd/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/thread.hpp>
#include <uhd/version.hpp>
#include <unistd.h>

extern "C" {
    #include "usrp_lib.h"
    #include "assertions.h"
    #include "log.h"
    #include "thread_create.h"
}

#include <simde/x86/avx2.h>
#include <simde/x86/fma.h>


/** @addtogroup _USRP_PHY_RF_INTERFACE_
 * @{
 */
int gpio789 = 0;
extern int usrp_tx_thread;
extern int usrp_capture_thread;
extern int usrp_cap_by_timestamp;

typedef struct {
    // --------------------------------
    // variables for USRP configuration
    // --------------------------------
    //! USRP device pointer
    uhd::usrp::multi_usrp::sptr usrp;

    // create a send streamer and a receive streamer
    //! USRP TX Stream
    uhd::tx_streamer::sptr tx_stream;
    //! USRP RX Stream
    uhd::rx_streamer::sptr rx_stream;

    //! USRP TX Metadata
    uhd::tx_metadata_t tx_md;
    //! USRP RX Metadata
    uhd::rx_metadata_t rx_md;

    //! Sampling rate
    double sample_rate;

    //! TX forward samples. We use usrp_time_offset to get this value
    int tx_forward_nsamps; // 166 for 20Mhz

    // --------------------------------
    // Debug and output control
    // --------------------------------
    int num_underflows;
    int num_overflows;
    int num_seq_errors;
    int64_t tx_count;
    int64_t rx_count;
    int wait_for_first_pps;
    int use_gps;
    // int first_tx;
    // int first_rx;
    //! timestamp of RX packet
    hw_timestamp rx_timestamp;
} usrp_state_t;

// void print_notes(void)
//{
//  Helpful notes
//   std::cout << boost::format("**************************************Helpful Notes on Clock/PPS
//   Selection**************************************\n"); std::cout << boost::format("As you can
//   see, the default 10 MHz Reference and 1 PPS signals are now from the GPSDO.\n"); std::cout <<
//   boost::format("If you would like to use the internal reference(TCXO) in other applications, you
//   must configure that explicitly.\n"); std::cout << boost::format("You can no longer select the
//   external SMAs for 10 MHz or 1 PPS signaling.\n"); std::cout <<
//   boost::format("****************************************************************************************************************\n");
// }

int check_ref_locked(usrp_state_t *s, size_t mboard)
{
    std::vector<std::string> sensor_names = s->usrp->get_mboard_sensor_names(mboard);
    bool ref_locked = false;

    if (std::find(sensor_names.begin(), sensor_names.end(), "ref_locked") != sensor_names.end()) {
        std::cout << "Waiting for reference lock..." << std::flush;

        for (int i = 0; i < 30 and not ref_locked; i++) {
            ref_locked = s->usrp->get_mboard_sensor("ref_locked", mboard).to_bool();

            if (not ref_locked) {
                std::cout << "." << std::flush;
                boost::this_thread::sleep(boost::posix_time::seconds(1));
            }
        }

        if (ref_locked) {
            std::cout << "LOCKED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    } else {
        std::cout << boost::format("ref_locked sensor not present on this board.\n");
    }

    return ref_locked;
}

static int sync_to_gps(hw_device *device)
{
    // uhd::set_thread_priority_safe();
    usrp_state_t *s = (usrp_state_t *)device->priv;

    try {
        size_t num_mboards = s->usrp->get_num_mboards();
        size_t num_gps_locked = 0;

        for (size_t mboard = 0; mboard < num_mboards; mboard++) {
            std::cout << "Synchronizing mboard " << mboard << ": "
                      << s->usrp->get_mboard_name(mboard) << std::endl;
            bool ref_locked = check_ref_locked(s, mboard);

            if (ref_locked) {
                std::cout << boost::format("Ref Locked\n");
            } else {
                std::cout << "Failed to lock to GPSDO 10 MHz Reference. Exiting." << std::endl;
                exit(EXIT_FAILURE);
            }

            // Wait for GPS lock
            bool gps_locked = s->usrp->get_mboard_sensor("gps_locked", mboard).to_bool();

            if (gps_locked) {
                num_gps_locked++;
                std::cout << boost::format("GPS Locked\n");
            } else {
                LOG_W("WARNING:  GPS not locked - time will not be accurate until locked\n");
            }

            // Set to GPS time
            uhd::time_spec_t gps_time =
                uhd::time_spec_t(time_t(s->usrp->get_mboard_sensor("gps_time", mboard).to_int()));
            // s->usrp->set_time_next_pps(gps_time+1.0, mboard);
            s->usrp->set_time_next_pps(uhd::time_spec_t(0.0));
            // Wait for it to apply
            // The wait is 2 seconds because N-Series has a known issue where
            // the time at the last PPS does not properly update at the PPS edge
            // when the time is actually set.
            boost::this_thread::sleep(boost::posix_time::seconds(2));
            // Check times
            gps_time =
                uhd::time_spec_t(time_t(s->usrp->get_mboard_sensor("gps_time", mboard).to_int()));
            uhd::time_spec_t time_last_pps = s->usrp->get_time_last_pps(mboard);
            std::cout << "USRP time: " << (boost::format("%0.9f") % time_last_pps.get_real_secs())
                      << std::endl;
            std::cout << "GPSDO time: " << (boost::format("%0.9f") % gps_time.get_real_secs())
                      << std::endl;
            if (gps_time.get_real_secs() == time_last_pps.get_real_secs()) {
                std::cout << std::endl
                          << "SUCCESS: USRP time synchronized to GPS time" << std::endl
                          << std::endl;
            } else {
                std::cerr << std::endl
                          << "ERROR: Failed to synchronize USRP time to GPS time" << std::endl
                          << std::endl;
            }
        }

        if (num_gps_locked == num_mboards and num_mboards > 1) {
            // Check to see if all USRP times are aligned
            // First, wait for PPS.
            uhd::time_spec_t time_last_pps = s->usrp->get_time_last_pps();

            while (time_last_pps == s->usrp->get_time_last_pps()) {
                boost::this_thread::sleep(boost::posix_time::milliseconds(1));
            }

            // Sleep a little to make sure all devices have seen a PPS edge
            boost::this_thread::sleep(boost::posix_time::milliseconds(200));
            // Compare times across all mboards
            bool all_matched = true;
            uhd::time_spec_t mboard0_time = s->usrp->get_time_last_pps(0);

            for (size_t mboard = 1; mboard < num_mboards; mboard++) {
                uhd::time_spec_t mboard_time = s->usrp->get_time_last_pps(mboard);

                if (mboard_time != mboard0_time) {
                    all_matched = false;
                    std::cerr << (boost::format(
                                      "ERROR: Times are not aligned: USRP 0=%0.9f, USRP %d=%0.9f") %
                                  mboard0_time.get_real_secs() % mboard %
                                  mboard_time.get_real_secs())
                              << std::endl;
                }
            }

            if (all_matched) {
                std::cout << "SUCCESS: USRP times aligned" << std::endl << std::endl;
            } else {
                std::cout << "ERROR: USRP times are not aligned" << std::endl << std::endl;
            }
        }
    } catch (std::exception &e) {
        std::cout << boost::format("\nError: %s") % e.what();
        std::cout << boost::format(
            "This could mean that you have not installed the GPSDO correctly.\n\n");
        std::cout << boost::format("Visit one of these pages if the problem persists:\n");
        std::cout << boost::format(" * N2X0/E1X0: http://files.ettus.com/manual/page_gpsdo.html");
        std::cout << boost::format(
            " * X3X0: http://files.ettus.com/manual/page_gpsdo_x3x0.html\n\n");
        std::cout << boost::format(
            " * E3X0: http://files.ettus.com/manual/page_usrp_e3x0.html#e3x0_hw_gps\n\n");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

/*! \brief Called to start the USRP transceiver. Return 0 if OK, < 0 if error
    @param device pointer to the device structure specific to the RF hardware target
*/
static int trx_usrp_start(hw_device *device)
{
    usrp_state_t *s = (usrp_state_t *)device->priv;

    // setup GPIO for TDD, GPIO(4) = ATR_RX
    // set data direction register (DDR) to output
    s->usrp->set_gpio_attr("FP0", "DDR", 0xfff, 0xfff);
    // set lower 7 bits to be controlled automatically by ATR (the rest 5 bits are controlled
    // manually)
    s->usrp->set_gpio_attr("FP0", "CTRL", 0x7f, 0xfff);
    // set pins 4 (RX_TX_Switch) and 6 (Shutdown PA) to 1 when the radio is only receiving (ATR_RX)
    s->usrp->set_gpio_attr("FP0", "ATR_RX", (1 << 4) | (1 << 6), 0x7f);
    // set pin 5 (Shutdown LNA) to 1 when the radio is transmitting and receiveing (ATR_XX)
    // (we use full duplex here, because our RX is on all the time - this might need to change
    // later)
    s->usrp->set_gpio_attr("FP0", "ATR_XX", (1 << 5), 0x7f);
    // set the output pins to 1
    s->usrp->set_gpio_attr("FP0", "OUT", 7 << 7, 0xf80);

    s->wait_for_first_pps = 1;
    s->rx_count = 0;
    s->tx_count = 0;
    // s->first_tx = 1;
    // s->first_rx = 1;
    s->rx_timestamp = 0;

    // wait for next pps
    uhd::time_spec_t last_pps = s->usrp->get_time_last_pps();
    uhd::time_spec_t current_pps = s->usrp->get_time_last_pps();
    while (current_pps == last_pps) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(1));
        current_pps = s->usrp->get_time_last_pps();
    }

    LOG_I(
          "current pps at %f, starting streaming at %f\n",
          current_pps.get_real_secs(),
          current_pps.get_real_secs() + 1.0);

    uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    cmd.time_spec = uhd::time_spec_t(current_pps + 1.0);
    cmd.stream_now = false; // start at constant delay
    s->rx_stream->issue_stream_cmd(cmd);

    return 0;
}

static void trx_usrp_send_end_of_burst(usrp_state_t *s)
{
    // if last packet sent was end of burst no need to do anything. otherwise send end of burst
    // packet
    if (s->tx_md.end_of_burst) {
        return;
    }
    s->tx_md.end_of_burst = true;
    s->tx_md.start_of_burst = false;
    s->tx_md.has_time_spec = false;
    s->tx_stream->send("", 0, s->tx_md);
}

static void trx_usrp_finish_rx(usrp_state_t *s)
{
    /* finish rx by sending STREAM_MODE_STOP_CONTINUOUS */
    uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    s->rx_stream->issue_stream_cmd(cmd);

    /* collect all remaining samples (not sure if needed) */
    size_t samples;
    uint8_t buf[1024];
    std::vector<void *> buff_ptrs;
    for (size_t i = 0; i < s->usrp->get_rx_num_channels(); i++) {
        buff_ptrs.push_back(buf);
    }
    do {
        samples = s->rx_stream->recv(buff_ptrs, sizeof(buf) / 4, s->rx_md);
    } while (samples > 0);
}

static void trx_usrp_write_reset(hw_thread_t *wt);

/*! \brief Terminate operation of the USRP transceiver -- free all associated resources
 * \param device the hardware to use
 */
static void trx_usrp_end(hw_device *device)
{
    if (device == NULL) {
        return;
    }

    usrp_state_t *s = (usrp_state_t *)device->priv;

    AssertFatal(s != NULL, "%s() called on uninitialized USRP\n", __func__);
    // iqrecorder_end(device);

    LOG_I("releasing USRP\n");
    if (usrp_tx_thread != 0) {
        trx_usrp_write_reset(&device->write_thread);
    }

    /* finish tx and rx */
    if (s->tx_count > 0) {
        trx_usrp_send_end_of_burst(s);
    }
    if (s->rx_count > 0) {
        trx_usrp_finish_rx(s);
    }
    /* set tx_stream, rx_stream, and usrp to NULL to clear/free them */
    s->tx_stream = NULL;
    s->rx_stream = NULL;
    s->usrp = NULL;
    free(s);
    device->priv = NULL;
    device->trx_start_func = NULL;
    device->trx_get_stats_func = NULL;
    device->trx_reset_stats_func = NULL;
    device->trx_end_func = NULL;
    device->trx_stop_func = NULL;
    device->trx_set_freq_func = NULL;
    device->trx_set_gains_func = NULL;
    device->trx_write_init = NULL;
}

// #define write_usrp
// #define read_usrp

/*! \brief Called to send samples to the USRP RF target
      @param device pointer to the device structure specific to the RF hardware target
      @param timestamp The timestamp at which the first sample MUST be sent
      @param buff Buffer which holds the samples
      @param nsamps number of samples to be sent
      @param antenna_id index of the antenna if the device has multiple antennas
      @param flags flags must be set to TRUE if timestamp parameter needs to be applied
*/
static int trx_usrp_write(hw_device *device,
                          hw_timestamp timestamp,
                          void **buff,
                          int nsamps,
                          int cc,
                          int flags)
{
#ifdef write_usrp
    char filename[] = "/tmp/usrp_tx.pcm";
    // LOG_I(filename, "/tmp/usrp/write/write%lld.pcm", timestamp);
    FILE *file = fopen(filename, "a+");
#endif
    int ret = 0;
    usrp_state_t *s = (usrp_state_t *)device->priv;
    int nsamps2; // aligned to upper 32 or 16 byte boundary

    int flags_lsb = flags & 0xff;
    int flags_msb = (flags >> 8) & 0xff;

    int end;
    hw_thread_t *write_thread = &device->write_thread;
    hw_write_package_t *write_package = write_thread->write_package;

    AssertFatal(MAX_WRITE_THREAD_BUFFER_SIZE >= cc,
                "Do not support more than %d cc number\n",
                MAX_WRITE_THREAD_BUFFER_SIZE);

    bool first_packet_state = false, last_packet_state = false;

    if (flags_lsb == 2) { // start of burst
        //      s->tx_md.start_of_burst = true;
        //      s->tx_md.end_of_burst = false;
        first_packet_state = true;
        last_packet_state = false;
    } else if (flags_lsb == 3) { // end of burst
        // s->tx_md.start_of_burst = false;
        // s->tx_md.end_of_burst = true;
        first_packet_state = false;
        last_packet_state = true;
    } else if (flags_lsb == 4) { // start and end
        // s->tx_md.start_of_burst = true;
        // s->tx_md.end_of_burst = true;
        first_packet_state = true;
        last_packet_state = true;
    } else if (flags_lsb == 1) { // middle of burst
        // s->tx_md.start_of_burst = false;
        // s->tx_md.end_of_burst = false;
        first_packet_state = false;
        last_packet_state = false;
    } else if (flags_lsb == 10) { // fail safe mode
        // s->tx_md.has_time_spec = false;
        // s->tx_md.start_of_burst = false;
        // s->tx_md.end_of_burst = true;
        first_packet_state = false;
        last_packet_state = true;
    }


    if ((usrp_capture_thread & 2) > 0) {
        hw_capture_thread_t *capture_thread = &device->tx_capture_thread;
        hw_write_package_t *write_package = capture_thread->write_package;
        pthread_mutex_lock(&capture_thread->mutex_write);

        if (capture_thread->count_write >= MAX_CAPTURE_THREAD_PACKAGE) {
            LOG_E(
                "Tx Capture Buffer overflow, count_write = %d, start = %d end = %d, resetting write package\n",
                capture_thread->count_write,
                capture_thread->start,
                capture_thread->end);
            capture_thread->end = capture_thread->start;
            capture_thread->count_write = 0;
            abort();//"todo"
        }

        end = capture_thread->end;
        write_package[end].timestamp = timestamp;
        write_package[end].nsamps = nsamps;
        static int nsamps_static_tx = 0;
        if (capture_thread->end == 0) {
            nsamps_static_tx = 0;
        }
        // write_package[end].cc = cc;
        for (int i = 0; i < cc; i++) {
            write_package[end].buff[i] = buff[i];
        }
        capture_thread->count_write++;
        capture_thread->end = (capture_thread->end + 1) % MAX_CAPTURE_THREAD_PACKAGE;

        LOG_D("Signaling TX capture TS %llu nsamps_static_tx %d buff ptr %p nsamps %d count %d end %d\n", (unsigned long long)timestamp, nsamps_static_tx, write_package[end].buff[0], nsamps, capture_thread->count_write, end);
        
        pthread_cond_signal(&capture_thread->cond_write);
        pthread_mutex_unlock(&capture_thread->mutex_write);
    }

    if (usrp_tx_thread == 0) {
        nsamps2 = (nsamps + 7) >> 3;
        simde__m256i buff_tx[cc < 2 ? 2 : cc][nsamps2];
        // bring RX data into 12 LSBs for softmodem RX
        for (int i = 0; i < cc; i++) {
            for (int j = 0; j < nsamps2; j++) {
                if ((((uintptr_t)buff[i]) & 0x1F) == 0) {
                    buff_tx[i][j] = simde_mm256_slli_epi16(((simde__m256i *)buff[i])[j], 4);
                } else {
                  simde__m256i tmp = simde_mm256_loadu_si256(((simde__m256i *)buff[i]) + j);
                    buff_tx[i][j] = simde_mm256_slli_epi16(tmp, 4);
                }
            }
        }

        s->tx_md.has_time_spec = true;
        s->tx_md.start_of_burst = (s->tx_count == 0) ? true : first_packet_state;
        s->tx_md.end_of_burst = last_packet_state;
        s->tx_md.time_spec = uhd::time_spec_t::from_ticks(timestamp, s->sample_rate);
        s->tx_count++;

        // bit 3 enables gpio (for backward compatibility)
        if (flags_msb & 8) {
            // push GPIO bits 7-9 from flags_msb
            int gpio789 = (flags_msb & 7) << 7;
            s->usrp->set_command_time(s->tx_md.time_spec);
            s->usrp->set_gpio_attr("FP0", "OUT", gpio789, 0x380);
            s->usrp->clear_command_time();
        }

        if (cc > 1) {
            std::vector<void *> buff_ptrs;

            for (int i = 0; i < cc; i++) {
                buff_ptrs.push_back(&(((int16_t *)buff_tx[i])[0]));
            }

            ret = (int)s->tx_stream->send(buff_ptrs, nsamps, s->tx_md);
        } else {
            ret = (int)s->tx_stream->send(&(((int16_t *)buff_tx[0])[0]), nsamps, s->tx_md);
        }
        // LOG_I("cc %d flags %d nsamps %d nsamps2 %d s->tx_count %d\n", cc, flags, nsamps,
        // nsamps2, s->tx_count);

#ifdef write_usrp
        fwrite(&((int16_t *)buff_tx[0])[0], nsamps * sizeof(int), 1, file);
        fclose(file);
#endif
        memset(&((int *)buff_tx[0])[0], 0, nsamps*sizeof(int));

        if (ret != nsamps) {
            LOG_E("[xmit] tx samples %d != %d\n", ret, nsamps);
        }
        return ret;
    } else {
        pthread_mutex_lock(&write_thread->mutex_write);

        if (write_thread->count_write >= MAX_WRITE_THREAD_PACKAGE) {
            LOG_E(
                "Buffer overflow, count_write = %d, start = %d end = %d, resetting write package\n",
                write_thread->count_write,
                write_thread->start,
                write_thread->end);
            write_thread->end = write_thread->start;
            // todo..
            abort();
            write_thread->count_write = 0;
        }

        end = write_thread->end;
        write_package[end].timestamp = timestamp;
        write_package[end].nsamps = nsamps;
        write_package[end].cc = cc;
        write_package[end].first_packet = first_packet_state;
        write_package[end].last_packet = last_packet_state;
        write_package[end].flags_msb = flags_msb;
        for (int i = 0; i < cc; i++) {
            write_package[end].buff[i] = buff[i];
        }

#ifdef write_usrp
        fwrite(&((int *)buff[0])[0], nsamps * sizeof(int), 1, file);
        fclose(file);
#endif
        write_thread->count_write++;
        write_thread->end = (write_thread->end + 1) % MAX_WRITE_THREAD_PACKAGE;
        LOG_D("Signaling TX TS %llu\n", (unsigned long long)timestamp);
        pthread_cond_signal(&write_thread->cond_write);
        pthread_mutex_unlock(&write_thread->mutex_write);
        return 0;
    }
}

//-----------------------start--------------------------
/*! \brief Called to send samples to the USRP RF target
      @param device pointer to the device structure specific to the RF hardware target
      @param timestamp The timestamp at which the first sample MUST be sent
      @param buff Buffer which holds the samples
      @param nsamps number of samples to be sent
      @param antenna_id index of the antenna if the device has multiple antennas
      @param flags flags must be set to TRUE if timestamp parameter needs to be applied
*/
void *trx_usrp_write_thread(void *arg)
{
    int ret = 0;
    hw_device *device = (hw_device *)arg;
    hw_thread_t *write_thread = &device->write_thread;
    hw_write_package_t *write_package = write_thread->write_package;

    usrp_state_t *s;
    int nsamps2; // aligned to upper 32 or 16 byte boundary
    int start;
    hw_timestamp timestamp;
    void **buff;
    int nsamps;
    int cc;
    signed char first_packet;
    signed char last_packet;
    int flags_msb;

    // char filename[] = "/tmp/usrp_tx_thread.pcm";
    // FILE *file = fopen(filename, "a+");

    while (1) {
        pthread_mutex_lock(&write_thread->mutex_write);
        while (write_thread->count_write == 0) {
            pthread_cond_wait(&write_thread->cond_write,
                              &write_thread->mutex_write); // this unlocks mutex_rxtx while waiting
                                                           // and then locks it again
        }
        if (write_thread->write_thread_exit) {
            break;
        }
        s = (usrp_state_t *)device->priv;
        start = write_thread->start;
        timestamp = write_package[start].timestamp;
        buff = write_package[start].buff;
        nsamps = write_package[start].nsamps;
        cc = write_package[start].cc;
        first_packet = write_package[start].first_packet;
        last_packet = write_package[start].last_packet;
        flags_msb = write_package[start].flags_msb;
        write_thread->start = (write_thread->start + 1) % MAX_WRITE_THREAD_PACKAGE;
        write_thread->count_write--;
        pthread_mutex_unlock(&write_thread->mutex_write);
        // if (write_thread->count_write != 0) {
        //   LOG_W("count write = %d, start = %d, end = %d nsamps %d\n",
        //   write_thread->count_write, write_thread->start,
        //         write_thread->end, nsamps);
        // }

        nsamps2 = (nsamps + 7) >> 3;
        simde__m256i buff_tx[cc < 2 ? 2 : cc][nsamps2];

        // bring RX data into 12 LSBs for softmodem RX
        for (int i = 0; i < cc; i++) {
            for (int j = 0; j < nsamps2; j++) {
                if ((((uintptr_t)buff[i]) & 0x1F) == 0) {
                    buff_tx[i][j] = simde_mm256_slli_epi16(((simde__m256i *)buff[i])[j], 4);
                } else {
                    simde__m256i tmp = simde_mm256_loadu_si256(((simde__m256i *)buff[i]) + j);
                    buff_tx[i][j] = simde_mm256_slli_epi16(tmp, 4);
                }
            }
        }

        s->tx_md.has_time_spec = true;
        s->tx_md.start_of_burst = (s->tx_count == 0) ? true : first_packet;
        s->tx_md.end_of_burst = last_packet;
        s->tx_md.time_spec = uhd::time_spec_t::from_ticks(timestamp, s->sample_rate);
        LOG_D("usrp_tx_write: tx_count %llu SoB %d, EoB %d, TS %llu\n",
              (unsigned long long)s->tx_count,
              s->tx_md.start_of_burst,
              s->tx_md.end_of_burst,
              (unsigned long long)timestamp);
        s->tx_count++;

        // bit 3 enables gpio (for backward compatibility)
        if (flags_msb & 8) {
            // push GPIO bits 7-9 from flags_msb
            int gpio789 = (flags_msb & 7) << 7;
            s->usrp->set_command_time(s->tx_md.time_spec);
            s->usrp->set_gpio_attr("FP0", "OUT", gpio789, 0x380);
            s->usrp->clear_command_time();
        }

        if (cc > 1) {
            std::vector<void *> buff_ptrs;

            for (int i = 0; i < cc; i++) {
                buff_ptrs.push_back(&(((int16_t *)buff_tx[i])[0]));
            }

            ret = (int)s->tx_stream->send(buff_ptrs, nsamps, s->tx_md);
        } else {
            ret = (int)s->tx_stream->send(&(((int16_t *)buff_tx[0])[0]), nsamps, s->tx_md);
        }

        if (ret != nsamps) {
            LOG_E("[xmit] tx samples %d != %d\n", ret, nsamps);
        }

        // fwrite(&((int16_t *)buff_tx[0])[0], nsamps*sizeof(int), 1, file);
        memset(&((int *)buff[0])[0], 0, nsamps*sizeof(int));
    }
    // fclose(file);

    return NULL;
}

int trx_usrp_write_init(hw_device *device)
{
    // uhd::set_thread_priority_safe(1.0);
    int affinity_trx_write_thread = -1;
    hw_thread_t *write_thread = &device->write_thread;
    LOG_I("initializing tx write thread\n");

    write_thread->start = 0;
    write_thread->end = 0;
    write_thread->count_write = 0;
    write_thread->write_thread_exit = false;

    pthread_mutex_init(&write_thread->mutex_write, NULL);
    pthread_cond_init(&write_thread->cond_write, NULL);
#if CPU_AFFINITY
    affinity_trx_write_thread = CPU_AFFINITY_USRP_WRITE;
#endif

    threadCreate(&write_thread->pthread_write,
                 trx_usrp_write_thread,
                 (void *)device,
                 "trx_usrp_write_thread",
                 affinity_trx_write_thread,
                 sched_get_priority_max(SCHED_RR));

    return (0);
}

void *trx_usrp_capture_thread(void *arg)
{
    hw_capture_thread_t *capture_thread = (hw_capture_thread_t *)arg;
    
    FILE *file = NULL;
    // int fd = 0;
    int cap_choice = capture_thread->is_rxtx;
    char filename[256];

    if (usrp_cap_by_timestamp == 0) {
        file = fopen(capture_thread->iq_record_path, "w");
        if (file == NULL) {
            fprintf(stderr, "Error opening file!\n");
            abort();
        }
    }

    int start;
    hw_timestamp timestamp;
    void **buff;
    int nsamps;
    while (1) {
        pthread_mutex_lock(&capture_thread->mutex_write);
        while (capture_thread->count_write < capture_thread->write_chunk) {
            pthread_cond_wait(&capture_thread->cond_write,
                              &capture_thread->mutex_write); // this unlocks mutex_rxtx while waiting
                                                           // and then locks it again
        }
        if (capture_thread->rxtx_capture_exit) {
            break;
        }

        hw_write_package_t *write_package = capture_thread->write_package;
        start = capture_thread->start;
        timestamp = write_package[start].timestamp;
        buff = write_package[start].buff;
        nsamps = 0;
        for (int i = 0; i < capture_thread->write_chunk; i++) { 
            nsamps += write_package[start + i].nsamps;
        }
        // nsamps = write_package[start].nsamps;
    
        capture_thread->start = (capture_thread->start + capture_thread->write_chunk) % MAX_CAPTURE_THREAD_PACKAGE;
        capture_thread->count_write -= capture_thread->write_chunk;
        // if (capture_thread->count_write != 0) {
        //     LOG_E(">>>>>>>>>>>>>>>>>>>>>>>>>>>>> capture_thread->count_write = %d\n", capture_thread->count_write);
        // }
        // int skip_trashed = write_package[start].buff[0] == write_package[start + 1].buff[0];
        pthread_mutex_unlock(&capture_thread->mutex_write);

        LOG_D("Get RX capture TS %llu buff ptr %p nsamps %d count %d start %d\n", (unsigned long long)timestamp, buff[0], nsamps, capture_thread->count_write, start);

        // if (skip_trashed) {
        //     LOG_D(">>>> skip trashed frames!\n");
        //     continue;
        // }

        if (usrp_cap_by_timestamp == 1) {
            if (cap_choice == 0) {
                snprintf(filename, sizeof(filename), "/tmp/rx/usrp_rx%ld.pcm", timestamp);
            } else {
                snprintf(filename, sizeof(filename), "/tmp/tx/usrp_tx%ld.pcm", timestamp);
            }
            file = fopen(capture_thread->iq_record_path, "w");
            fwrite(buff[0], nsamps*sizeof(int), 1, file);
            fclose(file);
        } else {
            int writed_b = fwrite(buff[0], nsamps*sizeof(int), 1, file);
            if (writed_b != 1) {
                LOG_E(">>>>>>>>>>>>>>>>>>>>>>>>> writed_b %d nsamps %d\n", writed_b, nsamps);
            }
            // write(fd, &((int *)buff[0])[0], nsamps*sizeof(int));
        }
    }
    if (usrp_cap_by_timestamp == 0) {
        fclose(file);
        // close(fd);
    }
    return NULL;
}
int *capture_iq_buf;
int trx_usrp_capture_init(hw_device *device)
{
    // uhd::set_thread_priority_safe(1.0);
    for (int rxtx = 0; rxtx < 2; rxtx++) {
        if (((usrp_capture_thread >> rxtx) & 1) == 0) {
            continue;
        }
        hw_capture_thread_t *capture_raw = NULL;
        if (rxtx == 0) {
            capture_raw = &device->rx_capture_thread;
            LOG_I("initializing rx capture thread\n");
        } else {
            capture_raw = &device->tx_capture_thread;
            LOG_I("initializing tx capture thread\n");
        }
        // Из-за особенности начальной синхронизации необходим дополнительный буфер для rx
        if (rxtx == 0) {
            capture_iq_buf = (int *)calloc(1, 1228800*sizeof(int));
        }
        capture_raw->start = 0;
        capture_raw->end = 0;
        capture_raw->count_write = 0;
        capture_raw->rxtx_capture_exit = false;
        capture_raw->is_rxtx = rxtx;

        pthread_mutex_init(&capture_raw->mutex_write, NULL);
        pthread_cond_init(&capture_raw->cond_write, NULL);

        threadCreate(&capture_raw->pthread_write,
                    trx_usrp_capture_thread,
                    (void *)capture_raw,
                    rxtx == 0 ? "rx_cap_iq" : "tx_cap_iq",
                    -1,
                    PRIORITY_RT);
    }

    return (0);
}

static void trx_usrp_write_reset(hw_thread_t *wt)
{
    pthread_mutex_lock(&wt->mutex_write);
    wt->count_write = 1;
    wt->write_thread_exit = true;
    pthread_cond_signal(&wt->cond_write);
    pthread_mutex_unlock(&wt->mutex_write);
    void *retval = NULL;
    pthread_join(wt->pthread_write, &retval);
    LOG_I("stopped USRP write thread\n");
}

//---------------------end-------------------------

/*! \brief Receive samples from hardware.
 * Read \ref nsamps samples from each channel to buffers. buff[0] is the array for
 * the first channel. *ptimestamp is the time at which the first sample
 * was received.
 * \param device the hardware to use
 * \param[out] ptimestamp the time at which the first sample was received.
 * \param[out] buff An array of pointers to buffers for received samples. The buffers must be large
 * enough to hold the number of samples \ref nsamps. \param nsamps Number of samples. One sample is
 * 2 byte I + 2 byte Q => 4 byte. \param antenna_id Index of antenna for which to receive samples
 * \returns the number of sample read
 */
static int trx_usrp_read(hw_device *device,
                         hw_timestamp *ptimestamp,
                         void **buff,
                         int nsamps,
                         int cc)
{
    usrp_state_t *s = (usrp_state_t *)device->priv;
    int samples_received = 0;
    int nsamps2; // aligned to upper 32 or 16 byte boundary
    nsamps2 = (nsamps + 7) >> 3;
    simde__m256i buff_tmp[cc < 2 ? 2 : cc][nsamps2];


    int rxshift;
    switch (device->type) {
        case USRP_B200_DEV:
            rxshift = 4;
            break;
        case USRP_X300_DEV:
        case USRP_N300_DEV:
        case USRP_X400_DEV:
            rxshift = 2;
            break;
        default:
            AssertFatal(1 == 0, "Shouldn't be here\n");
    }

    samples_received = 0;
    while (samples_received != nsamps) {
        if (cc > 1) {
            // receive multiple channels (e.g. RF A and RF B)
            std::vector<void *> buff_ptrs;

            for (int i = 0; i < cc; i++) {
                buff_ptrs.push_back(buff_tmp[i] + samples_received);
            }

            samples_received += s->rx_stream->recv(buff_ptrs, nsamps, s->rx_md);
        } else {
            // receive a single channel (e.g. from connector RF A)

            samples_received +=
                s->rx_stream->recv((void *)((int32_t *)buff_tmp[0] + samples_received),
                                   nsamps - samples_received,
                                   s->rx_md);
        }
        if ((s->wait_for_first_pps == 0) &&
            (s->rx_md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE)) {
            break;
        }

        if ((s->wait_for_first_pps == 1) && (samples_received != nsamps)) {
            LOG_I("sleep...\n");
            usleep(100);
        }
    }
    if (samples_received == nsamps) {
        s->wait_for_first_pps = 0;
    }

    // bring RX data into 12 LSBs for softmodem RX
    for (int i = 0; i < cc; i++) {
        for (int j = 0; j < nsamps2; j++) {
            // FK: in some cases the buffer might not be 32 byte aligned, so we cannot use avx2

            if ((((uintptr_t)buff[i]) & 0x1F) == 0) {
                ((simde__m256i *)buff[i])[j] = simde_mm256_srai_epi16(buff_tmp[i][j], rxshift);
            } else {
              simde__m256i tmp = simde_mm256_srai_epi16(buff_tmp[i][j], rxshift);
                simde_mm256_storeu_si256(((simde__m256i *)buff[i]) + j, tmp);
            }
        }

    }

    if (samples_received < nsamps) {
        LOG_E("[recv] received %d samples out of %d\n", samples_received, nsamps);
    }
    if (s->rx_md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
        LOG_E("%s\n", s->rx_md.to_pp_string(true).c_str());
    }

    s->rx_count += nsamps;
    s->rx_timestamp = s->rx_md.time_spec.to_ticks(s->sample_rate);
    *ptimestamp = s->rx_timestamp;

#ifdef read_usrp
    FILE *file = fopen("/tmp/usrp_rxrx.pcm", "a+");
    fwrite(&((int *)buff[0])[0], nsamps * sizeof(int), 1, file);
    fclose(file);
#endif
    if ((usrp_capture_thread & 1) == 1) {
        static int nsamps_static = 0;
        hw_capture_thread_t *capture_thread = &device->rx_capture_thread;
        hw_write_package_t *write_package = capture_thread->write_package;
        memcpy(&capture_iq_buf[nsamps_static], &((int *)buff[0])[0], nsamps * sizeof(int));

        pthread_mutex_lock(&capture_thread->mutex_write);

        if (capture_thread->count_write >= MAX_CAPTURE_THREAD_PACKAGE) {
            LOG_E(
                "Rx Capture Buffer overflow, count_write = %d, start = %d end = %d, resetting write package\n",
                capture_thread->count_write,
                capture_thread->start,
                capture_thread->end);
            capture_thread->end = capture_thread->start;
            capture_thread->count_write = 0;
            abort(); // todo
        }

        int end = capture_thread->end;
        write_package[end].timestamp = *ptimestamp;
        write_package[end].nsamps = nsamps;

        // write_package[end].cc = cc;
        for (int i = 0; i < cc; i++) {
            write_package[end].buff[i] = &capture_iq_buf[nsamps_static];
        }
        nsamps_static += nsamps;

        capture_thread->count_write++;
        capture_thread->end = (capture_thread->end + 1) % MAX_CAPTURE_THREAD_PACKAGE;
        if (capture_thread->end == 0) {
            nsamps_static = 0;
        }
        LOG_D("Signaling RX capture TS %llu nsamps_static %d buff ptr %p nsamps %d count %d end %d\n", (unsigned long long)*ptimestamp, nsamps_static, write_package[end].buff[0], nsamps, capture_thread->count_write, end);
        pthread_cond_signal(&capture_thread->cond_write);
        pthread_mutex_unlock(&capture_thread->mutex_write);
    }

    return samples_received;
}

/*! \brief Compares two variables within precision
 * \param a first variable
 * \param b second variable
 */
static bool is_equal(double a, double b)
{
    return std::fabs(a - b) < std::numeric_limits<double>::epsilon();
}

void *freq_thread(void *arg)
{
    hw_device *device = (hw_device *)arg;
    usrp_state_t *s = (usrp_state_t *)device->priv;
    uhd::tune_request_t tx_tune_req(device->hw_config[0].tx_freq[0],
                                    device->hw_config[0].tune_offset);
    uhd::tune_request_t rx_tune_req(device->hw_config[0].rx_freq[0],
                                    device->hw_config[0].tune_offset);
    s->usrp->set_tx_freq(tx_tune_req);
    s->usrp->set_rx_freq(rx_tune_req);
    return NULL;
}

/*! \brief Set frequencies (TX/RX). Spawns a thread to handle the frequency change to not block the
 * calling thread \param device the hardware to use \param hw_config RF frontend parameters set by
 * application \param dummy dummy variable not used \returns 0 in success
 */
int trx_usrp_set_freq(hw_device *device, hw_config_t *hw_config)
{
    usrp_state_t *s = (usrp_state_t *)device->priv;
    LOG_D("Setting USRP TX Freq %d, RX Freq %d, tune_offset: %d\n",
          (int)hw_config[0].tx_freq[0],
          (int)hw_config[0].rx_freq[0],
          (int)hw_config[0].tune_offset);

    uhd::tune_request_t tx_tune_req(hw_config[0].tx_freq[0], hw_config[0].tune_offset);
    uhd::tune_request_t rx_tune_req(hw_config[0].rx_freq[0], hw_config[0].tune_offset);
    s->usrp->set_tx_freq(tx_tune_req);
    s->usrp->set_rx_freq(rx_tune_req);

    return (0);
}

/*! \brief Set RX frequencies
 * \param device the hardware to use
 * \param hw_config RF frontend parameters set by application
 * \returns 0 in success
 */
int hw_set_rx_frequencies(hw_device *device, hw_config_t *hw_config)
{
    usrp_state_t *s = (usrp_state_t *)device->priv;
    uhd::tune_request_t rx_tune_req(hw_config[0].rx_freq[0], hw_config[0].tune_offset);
    LOG_I(
          "In hw_set_rx_frequencies, freq: %f, tune offset: %f\n",
          hw_config[0].rx_freq[0],
          hw_config[0].tune_offset);
    // rx_tune_req.rf_freq_policy = uhd::tune_request_t::POLICY_MANUAL;
    // rx_tune_req.rf_freq = hw_config[0].rx_freq[0];
    s->usrp->set_rx_freq(rx_tune_req);
    return (0);
}

/*! \brief Set Gains (TX/RX)
 * \param device the hardware to use
 * \param hw_config RF frontend parameters set by application
 * \returns 0 in success
 */
int trx_usrp_set_gains(hw_device *device, hw_config_t *hw_config)
{
    usrp_state_t *s = (usrp_state_t *)device->priv;
    ::uhd::gain_range_t gain_range_tx = s->usrp->get_tx_gain_range(0);
    s->usrp->set_tx_gain(gain_range_tx.stop() - hw_config[0].tx_gain[0]);
    ::uhd::gain_range_t gain_range = s->usrp->get_rx_gain_range(0);

    // limit to maximum gain
    if (hw_config[0].rx_gain[0] - hw_config[0].rx_gain_offset[0] > gain_range.stop()) {
        LOG_E("RX Gain 0 too high, reduce by %f dB\n",
              hw_config[0].rx_gain[0] - hw_config[0].rx_gain_offset[0] - gain_range.stop());
        exit(-1);
    }

    s->usrp->set_rx_gain(hw_config[0].rx_gain[0] - hw_config[0].rx_gain_offset[0]);
    LOG_I(
          "Setting USRP RX gain to %f (rx_gain %f,gain_range.stop() %f)\n",
          hw_config[0].rx_gain[0] - hw_config[0].rx_gain_offset[0],
          hw_config[0].rx_gain[0],
          gain_range.stop());
    return (0);
}

/*! \brief Stop USRP
 * \param card refers to the hardware index to use
 */
void trx_usrp_stop(hw_device *device)
{
    if (device == NULL) {
        return;
    }
    usrp_state_t *s = (usrp_state_t *)device->priv;
    AssertFatal(s != NULL, "%s() called on uninitialized USRP\n", __func__);
    // iqrecorder_end(device);
    LOG_I("releasing USRP\n");
    if (usrp_tx_thread != 0) {
        trx_usrp_write_reset(&device->write_thread);
    }
    /* finish tx and rx */
    if (s->tx_count > 0) {
        trx_usrp_send_end_of_burst(s);
    }
    if (s->rx_count > 0) {
        trx_usrp_finish_rx(s);
    }
}

/*! \brief USRPB210 RX calibration table */
rx_gain_calib_table_t calib_table_b210[] = {{3500000000.0, 44.0},
                                            {2660000000.0, 49.0},
                                            {2300000000.0, 50.0},
                                            {1880000000.0, 53.0},
                                            {816000000.0, 58.0},
                                            {-1, 0}};

/*! \brief USRPB210 RX calibration table */
rx_gain_calib_table_t calib_table_b210_38[] = {{3500000000.0, 44.0},
                                               {2660000000.0, 49.8},
                                               {2300000000.0, 51.0},
                                               {1880000000.0, 53.0},
                                               {816000000.0, 57.0},
                                               {-1, 0}};

/*! \brief USRPx310 RX calibration table */
rx_gain_calib_table_t calib_table_x310[] = {{3500000000.0, 77.0},
                                            {2660000000.0, 81.0},
                                            {2300000000.0, 81.0},
                                            {1880000000.0, 82.0},
                                            {816000000.0, 85.0},
                                            {-1, 0}};

/*! \brief USRPn3xf RX calibration table */
rx_gain_calib_table_t calib_table_n310[] = {{3500000000.0, 0.0},
                                            {2660000000.0, 0.0},
                                            {2300000000.0, 0.0}, //-32.0
                                            {1880000000.0, 0.0},
                                            {816000000.0, 0.0},
                                            {-1, 0}};
/*! \brief Empty RX calibration table */
rx_gain_calib_table_t calib_table_none[] = {{3500000000.0, 0.0},
                                            {2660000000.0, 0.0},
                                            {2300000000.0, 0.0},
                                            {1880000000.0, 0.0},
                                            {816000000.0, 0.0},
                                            {-1, 0}};

/*! \brief Set RX gain offset
 * \param hw_config RF frontend parameters set by application
 * \param chain_index RF chain to apply settings to
 * \returns 0 in success
 */
void set_rx_gain_offset(hw_config_t *hw_config, int chain_index, int bw_gain_adjust)
{
    int i = 0;
    // loop through calibration table to find best adjustment factor for RX frequency
    double min_diff = 6e9, diff, gain_adj = 0.0;

    if (bw_gain_adjust == 1) {
        switch ((int)hw_config[0].sample_rate) {
            case 46080000:
                break;

            case 30720000:
                break;

            case 23040000:
                gain_adj = 1.25;
                break;

            case 15360000:
                gain_adj = 3.0;
                break;

            case 7680000:
                gain_adj = 6.0;
                break;

            case 3840000:
                gain_adj = 9.0;
                break;

            case 1920000:
                gain_adj = 12.0;
                break;

            default:
                LOG_E("unknown sampling rate %d\n", (int)hw_config[0].sample_rate);
                // exit(-1);
                break;
        }
    }

    while (hw_config->rx_gain_calib_table[i].freq > 0) {
        diff = fabs(hw_config->rx_freq[chain_index] - hw_config->rx_gain_calib_table[i].freq);
        LOG_I(
              "cal %d: freq %f, offset %f, diff %f\n",
              i,
              hw_config->rx_gain_calib_table[i].freq,
              hw_config->rx_gain_calib_table[i].offset,
              diff);

        if (min_diff > diff) {
            min_diff = diff;
            hw_config->rx_gain_offset[chain_index] =
                hw_config->rx_gain_calib_table[i].offset + gain_adj;
        }

        i++;
    }
}

/*! \brief print the USRP statistics
 * \returns  0 on success
 */
int trx_usrp_get_stats(hw_device *)
{
    return (0);
}

/*! \brief Reset the USRP statistics
 * \returns  0 on success
 */
int trx_usrp_reset_stats(hw_device *)
{
    return (0);
}

extern "C" {
    int device_init(hw_device *device, hw_config_t *hw_config)
    {                 
        LOG_I("hw_config[0].sdr_addrs == '%s'\n", hw_config[0].sdr_addrs);
        LOG_I(
              "hw_config[0].clock_source == '%d' (internal = %d, external = %d)\n",
              hw_config[0].clock_source,
              internal,
              external);
        usrp_state_t *s;
        int choffset = 0;

        if (device->priv == NULL) {
            s = (usrp_state_t *)calloc(sizeof(usrp_state_t), 1);
            device->priv = s;
            AssertFatal(s != NULL, "USRP device: memory allocation failure\n");
        } else {
            LOG_E("multiple device init detected\n");
            return 0;
        }

        device->hw_config = hw_config;
        device->trx_start_func = trx_usrp_start;
        device->trx_get_stats_func = trx_usrp_get_stats;
        device->trx_reset_stats_func = trx_usrp_reset_stats;
        device->trx_end_func = trx_usrp_end;
        device->trx_stop_func = trx_usrp_stop;
        device->trx_set_freq_func = trx_usrp_set_freq;
        device->trx_set_gains_func = trx_usrp_set_gains;
        device->trx_write_init = trx_usrp_write_init;
        device->trx_capture_init = trx_usrp_capture_init;

        // hotfix! to be checked later
        uhd::set_thread_priority_safe(1.0);
        // pthread_t thread = pthread_self();
        // Initialize USRP device
        int vers = 0, subvers = 0, subsubvers = 0;
        int bw_gain_adjust = 0;

        sscanf(uhd::get_version_string().c_str(), "%d.%d.%d", &vers, &subvers, &subsubvers);
        LOG_I(
              "UHD version %s (%d.%d.%d)\n",
              uhd::get_version_string().c_str(),
              vers,
              subvers,
              subsubvers);
        std::string args, tx_subdev, rx_subdev;

        if (hw_config[0].sdr_addrs == NULL) {
            args = "type=b200";
        } else {
            args = hw_config[0].sdr_addrs;
            LOG_I("Checking for USRP with args %s\n", hw_config[0].sdr_addrs);
        }

        uhd::device_addrs_t device_adds = uhd::device::find(args);

        if (device_adds.size() == 0) {
            LOG_E("No USRP Device Found.\n ");
            free(s);
            return -1;
        } else if (device_adds.size() > 1) {
            LOG_E("More than one USRP Device Found. Please specify device more precisely in config file.\n");
            free(s);
            return -1;
        }

        LOG_I("Found USRP %s\n", device_adds[0].get("type").c_str());
        double usrp_master_clock;

        if (device_adds[0].get("type") == "b200") {
            device->type = USRP_B200_DEV;
            usrp_master_clock = 30.72e6;
            args += boost::str(boost::format(",master_clock_rate=%f") % usrp_master_clock);
            args +=
                ",num_send_frames=256,num_recv_frames=256, send_frame_size=7680, "
                "recv_frame_size=7680";
        }

        if (device_adds[0].get("type") == "n3xx") {
            LOG_I("Found USRP n300\n");
            device->type = USRP_N300_DEV;
            usrp_master_clock = 122.88e6;
            args += boost::str(boost::format(",master_clock_rate=%f") % usrp_master_clock);

            if (0 != system("sysctl -w net.core.rmem_max=62500000 net.core.wmem_max=62500000")) {
                LOG_W("Can't set kernel parameters for N3x0\n");
            }
        }

        if (device_adds[0].get("type") == "x300") {
            LOG_I("Found USRP x300\n");
            device->type = USRP_X300_DEV;
            usrp_master_clock = 184.32e6;
            args += boost::str(boost::format(",master_clock_rate=%f") % usrp_master_clock);

            // USRP recommended: https://files.ettus.com/manual/page_usrp_x3x0_config.html
            if (0 != system("sysctl -w net.core.rmem_max=33554432 net.core.wmem_max=33554432")) {
                LOG_W("Can't set kernel parameters for X3xx\n");
            }
        }

        s->usrp = uhd::usrp::multi_usrp::make(args);

        if (args.find("clock_source") == std::string::npos) {
            if (hw_config[0].clock_source == internal) {
                s->usrp->set_clock_source("internal");
                LOG_I("Setting clock source to internal\n");
            } else if (hw_config[0].clock_source == external) {
                s->usrp->set_clock_source("external");
                LOG_I("Setting clock source to external\n");
            } else if (hw_config[0].clock_source == gpsdo) {
                s->usrp->set_clock_source("gpsdo");
                LOG_I("Setting clock source to gpsdo\n");
            } else {
                LOG_W("Clock source set neither in usrp_args nor on command line, using default!\n");
            }
        } else {
            if (hw_config[0].clock_source != unset) {
                LOG_W(
                      "Clock source set in both usrp_args and in clock_source, ingnoring the "
                      "latter!\n");
            }
        }

        if (args.find("time_source") == std::string::npos) {
            if (hw_config[0].time_source == internal) {
                s->usrp->set_time_source("internal");
                LOG_I("Setting time source to internal\n");
            } else if (hw_config[0].time_source == external) {
                s->usrp->set_time_source("external");
                LOG_I("Setting time source to external\n");
            } else if (hw_config[0].time_source == gpsdo) {
                s->usrp->set_time_source("gpsdo");
                LOG_I("Setting time source to gpsdo\n");
            } else {
                LOG_W(
                      "Time source set neither in usrp_args nor on command line, using default!\n");
            }
        } else {
            if (hw_config[0].clock_source != unset) {
                LOG_W(
                      "Time source set in both usrp_args and in time_source, ingnoring the "
                      "latter!\n");
            }
        }

        if (s->usrp->get_clock_source(0) == "gpsdo") {
            s->use_gps = 1;

            if (sync_to_gps(device) == EXIT_SUCCESS) {
                LOG_I("USRP synced with GPS!\n");
            } else {
                LOG_I("USRP fails to sync with GPS. Exiting.\n");
                exit(EXIT_FAILURE);
            }
        } else {
            s->usrp->set_time_next_pps(uhd::time_spec_t(0.0));

            if (s->usrp->get_clock_source(0) == "external") {
                if (check_ref_locked(s, 0)) {
                    LOG_I("USRP locked to external reference!\n");
                } else {
                    LOG_I("Failed to lock to external reference. Exiting.\n");
                    exit(EXIT_FAILURE);
                }
            }
        }

        if (device->type == USRP_X300_DEV) {
            hw_config[0].rx_gain_calib_table = calib_table_x310;
            LOG_I("-- Using calibration table: calib_table_x310\n");
        }

        if (device->type == USRP_N300_DEV) {
            hw_config[0].rx_gain_calib_table = calib_table_n310;
            LOG_I("-- Using calibration table: calib_table_n310\n");
        }

        if (device->type == USRP_N300_DEV) {
            s->usrp->set_rx_subdev_spec(::uhd::usrp::subdev_spec_t(hw_config[0].rx_subdev));
            LOG_I("Setting subdev (channel) Rx: %s\n", hw_config[0].rx_subdev);
            s->usrp->set_tx_subdev_spec(::uhd::usrp::subdev_spec_t(hw_config[0].tx_subdev));
            LOG_I("Setting subdev (channel) Tx: %s\n", hw_config[0].tx_subdev);            
        }

        if (device->type == USRP_N300_DEV || device->type == USRP_X300_DEV) {
            LOG_I("%s() sample_rate:%u\n", __FUNCTION__, (int)hw_config[0].sample_rate);

            switch ((int)hw_config[0].sample_rate) {
                case 122880000:
                    // from usrp_time_offset
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 15; // to be checked
                    hw_config[0].tx_bw = 80e6;
                    hw_config[0].rx_bw = 80e6;
                    break;

                case 92160000:
                    // from usrp_time_offset
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 15; // to be checked
                    hw_config[0].tx_bw = 80e6;
                    hw_config[0].rx_bw = 80e6;
                    break;

                case 61440000:
                    // from usrp_time_offset
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 15;
                    hw_config[0].tx_bw = 40e6;
                    hw_config[0].rx_bw = 40e6;
                    break;

                case 46080000:
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 15;
                    hw_config[0].tx_bw = 40e6;
                    hw_config[0].rx_bw = 40e6;
                    break;

                case 30720000:
                    // from usrp_time_offset
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 15;
                    hw_config[0].tx_bw = 20e6;
                    hw_config[0].rx_bw = 20e6;
                    break;

                case 15360000:
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 45;
                    hw_config[0].tx_bw = 10e6;
                    hw_config[0].rx_bw = 10e6;
                    break;

                case 7680000:
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 50;
                    hw_config[0].tx_bw = 5e6;
                    hw_config[0].rx_bw = 5e6;
                    break;

                case 1920000:
                    // hw_config[0].samples_per_packet    = 2048;
                    hw_config[0].tx_sample_advance = 50;
                    hw_config[0].tx_bw = 1.25e6;
                    hw_config[0].rx_bw = 1.25e6;
                    break;

                default:
                    LOG_E("Error: unknown sampling rate %f\n", hw_config[0].sample_rate);
                    exit(-1);
                    break;
            }
        }

        if (device->type == USRP_B200_DEV) {
            if ((vers == 3) && (subvers == 9) && (subsubvers >= 2)) {
                hw_config[0].rx_gain_calib_table = calib_table_b210;
                bw_gain_adjust = 0;
                std::cerr << "-- Using calibration table: calib_table_b210" << std::endl; // Bell
                                                                                          // Labs
                                                                                          // info
            } else {
                hw_config[0].rx_gain_calib_table = calib_table_b210_38;
                bw_gain_adjust = 1;
                std::cerr << "-- Using calibration table: calib_table_b210_38" << std::endl; // Bell
                                                                                             // Labs
                                                                                             // info
            }

            switch ((int)hw_config[0].sample_rate) {
                case 46080000:
                    s->usrp->set_master_clock_rate(46.08e6);
                    // hw_config[0].samples_per_packet    = 1024;
                    hw_config[0].tx_sample_advance = 115;
                    hw_config[0].tx_bw = 40e6;
                    hw_config[0].rx_bw = 40e6;
                    break;

                case 30720000:
                    s->usrp->set_master_clock_rate(30.72e6);
                    // hw_config[0].samples_per_packet    = 1024;
                    hw_config[0].tx_sample_advance = 115;
                    hw_config[0].tx_bw = 20e6;
                    hw_config[0].rx_bw = 20e6;
                    break;

                case 23040000:
                    s->usrp->set_master_clock_rate(23.04e6); // to be checked
                    // hw_config[0].samples_per_packet    = 1024;
                    hw_config[0].tx_sample_advance = 113;
                    hw_config[0].tx_bw = 20e6;
                    hw_config[0].rx_bw = 20e6;
                    break;

                case 15360000:
                    s->usrp->set_master_clock_rate(30.72e06);
                    // hw_config[0].samples_per_packet    = 1024;
                    hw_config[0].tx_sample_advance = 103;
                    hw_config[0].tx_bw = 20e6;
                    hw_config[0].rx_bw = 20e6;
                    break;

                case 7680000:
                    s->usrp->set_master_clock_rate(30.72e6);
                    // hw_config[0].samples_per_packet    = 1024;
                    hw_config[0].tx_sample_advance = 80;
                    hw_config[0].tx_bw = 20e6;
                    hw_config[0].rx_bw = 20e6;
                    break;

                case 1920000:
                    s->usrp->set_master_clock_rate(30.72e6);
                    // hw_config[0].samples_per_packet    = 1024;
                    hw_config[0].tx_sample_advance = 40;
                    hw_config[0].tx_bw = 20e6;
                    hw_config[0].rx_bw = 20e6;
                    break;

                default:
                    LOG_E("Error: unknown sampling rate %f\n", hw_config[0].sample_rate);
                    exit(-1);
                    break;
            }
        }

        /* device specific */
        // hw_config[0].txlaunch_wait = 1;//manage when TX processing is triggered
        // hw_config[0].txlaunch_wait_slotcount = 1; //manage when TX processing is triggered
        hw_config[0].iq_txshift = 4; // shift
        hw_config[0].iq_rxrescale = 15; // rescale iqs

        for (int i = 0; i < ((int)s->usrp->get_rx_num_channels()); i++) {
            if (i < hw_config[0].rx_num_channels) {
                s->usrp->set_rx_rate(hw_config[0].sample_rate, i + choffset);
                uhd::tune_request_t rx_tune_req(hw_config[0].rx_freq[i], hw_config[0].tune_offset);
                s->usrp->set_rx_freq(rx_tune_req, i + choffset);
                set_rx_gain_offset(&hw_config[0], i, bw_gain_adjust);
                ::uhd::gain_range_t gain_range = s->usrp->get_rx_gain_range(i + choffset);
                // limit to maximum gain
                double gain = hw_config[0].rx_gain[i] - hw_config[0].rx_gain_offset[i];
                if (gain > gain_range.stop()) {
                    LOG_E("RX Gain too high, lower by %f dB\n", gain - gain_range.stop());
                    gain = gain_range.stop();
                }

                s->usrp->set_rx_gain(gain, i + choffset);
                LOG_I(
                      "RX Gain %d %f (%f) => %f (max %f)\n",
                      i,
                      hw_config[0].rx_gain[i],
                      hw_config[0].rx_gain_offset[i],
                      hw_config[0].rx_gain[i] - hw_config[0].rx_gain_offset[i],
                      gain_range.stop());
            }
        }

        LOG_D("usrp->get_tx_num_channels() == %zd\n", s->usrp->get_tx_num_channels());
        LOG_D("hw_config[0].tx_num_channels == %d\n", hw_config[0].tx_num_channels);

        for (int i = 0; i < ((int)s->usrp->get_tx_num_channels()); i++) {
            ::uhd::gain_range_t gain_range_tx = s->usrp->get_tx_gain_range(i);

            if (i < hw_config[0].tx_num_channels) {
                s->usrp->set_tx_rate(hw_config[0].sample_rate, i + choffset);
                uhd::tune_request_t tx_tune_req(hw_config[0].tx_freq[i], hw_config[0].tune_offset);
                s->usrp->set_tx_freq(tx_tune_req, i + choffset);
                if (hw_config[0].tx_gain[i] > gain_range_tx.stop()) {
                    LOG_E("TX Gain %d too high, MAX Tx gain %d dB\n", 
                        hw_config[0].tx_gain[i],
                        gain_range_tx.stop());
                }
                s->usrp->set_tx_gain(gain_range_tx.stop() - hw_config[0].tx_gain[i], i+choffset);
                LOG_I(
                      "USRP TX_GAIN: %d %3.2lf gain_range:%3.2lf tx_gain:%3.2lf\n",
                      i,
                      gain_range_tx.stop() - hw_config[0].tx_gain[i],
                      gain_range_tx.stop(),
                      hw_config[0].tx_gain[i]);
            }
        }

        // s->usrp->set_clock_source("external");
        // s->usrp->set_time_source("external");
        //  display USRP settings
        LOG_I("Actual master clock: %fMHz...\n", s->usrp->get_master_clock_rate() / 1e6);
        LOG_I("Actual clock source %s...\n", s->usrp->get_clock_source(0).c_str());
        LOG_I("Actual time source %s...\n", s->usrp->get_time_source(0).c_str());

        // create tx & rx streamer
        uhd::stream_args_t stream_args_rx("sc16", "sc16");
        for (int i = 0; i < hw_config[0].rx_num_channels; i++) {
            LOG_I("setting rx channel %d\n", i + choffset);
            stream_args_rx.channels.push_back(i + choffset);
        }
        s->rx_stream = s->usrp->get_rx_stream(stream_args_rx);

        int samples = hw_config[0].sample_rate;
        int max = s->rx_stream->get_max_num_samps();
        samples /= 10000;
        LOG_I("RF board max packet size %u, size for 100µs jitter %d \n", max, samples);

        if (samples < max) {
            stream_args_rx.args["spp"] = str(boost::format("%d") % samples);
        }

        LOG_I("rx_max_num_samps %zu\n", s->rx_stream->get_max_num_samps());

        uhd::stream_args_t stream_args_tx("sc16", "sc16");

        for (int i = 0; i < hw_config[0].tx_num_channels; i++) {
            stream_args_tx.channels.push_back(i + choffset);
        }

        s->tx_stream = s->usrp->get_tx_stream(stream_args_tx);

        /* Setting TX/RX BW after streamers are created due to USRP calibration issue */
        for (int i = 0;
             i < ((int)s->usrp->get_tx_num_channels()) && i < hw_config[0].tx_num_channels;
             i++) {
            s->usrp->set_tx_bandwidth(hw_config[0].tx_bw, i + choffset);
        }

        for (int i = 0;
             i < ((int)s->usrp->get_rx_num_channels()) && i < hw_config[0].rx_num_channels;
             i++) {
            s->usrp->set_rx_bandwidth(hw_config[0].rx_bw, i + choffset);
        }

        for (int i = 0; i < hw_config[0].rx_num_channels; i++) {
            LOG_I("RX Channel %d\n", i);
            LOG_I(
                  "  Actual RX sample rate: %fMSps...\n",
                  s->usrp->get_rx_rate(i + choffset) / 1e6);
            LOG_I(
                  "  Actual RX frequency: %fGHz...\n",
                  s->usrp->get_rx_freq(i + choffset) / 1e9);
            LOG_I("  Actual RX gain: %f...\n", s->usrp->get_rx_gain(i + choffset));
            LOG_I(
                  "  Actual RX bandwidth: %fM...\n",
                  s->usrp->get_rx_bandwidth(i + choffset) / 1e6);
            LOG_I(
                  "  Actual RX antenna: %s...\n",
                  s->usrp->get_rx_antenna(i + choffset).c_str());
        }

        for (int i = 0; i < hw_config[0].tx_num_channels; i++) {
            LOG_I("TX Channel %d\n", i);
            LOG_I(
                  "  Actual TX sample rate: %fMSps...\n",
                  s->usrp->get_tx_rate(i + choffset) / 1e6);
            LOG_I(
                  "  Actual TX frequency: %fGHz...\n",
                  s->usrp->get_tx_freq(i + choffset) / 1e9);
            LOG_I("  Actual TX gain: %f...\n", s->usrp->get_tx_gain(i + choffset));
            LOG_I(
                  "  Actual TX bandwidth: %fM...\n",
                  s->usrp->get_tx_bandwidth(i + choffset) / 1e6);
            LOG_I(
                  "  Actual TX antenna: %s...\n",
                  s->usrp->get_tx_antenna(i + choffset).c_str());
            LOG_I("  Actual TX packet size: %lu\n", s->tx_stream->get_max_num_samps());
        }

        LOG_I("Device timestamp: %f...\n", s->usrp->get_time_now().get_real_secs());
        device->trx_write_func = trx_usrp_write;
        device->trx_read_func = trx_usrp_read;
        s->sample_rate = hw_config[0].sample_rate;

        // TODO:
        // init tx_forward_nsamps based usrp_time_offset ex
        if (is_equal(s->sample_rate, (double)30.72e6)) {
            s->tx_forward_nsamps = 176;
        }

        if (is_equal(s->sample_rate, (double)15.36e6)) {
            s->tx_forward_nsamps = 90;
        }

        if (is_equal(s->sample_rate, (double)7.68e6)) {
            s->tx_forward_nsamps = 50;
        }

        // uhd::set_thread_priority_safe(0.89);
        return 0;
    }

    /*@}*/
} /* extern c */
