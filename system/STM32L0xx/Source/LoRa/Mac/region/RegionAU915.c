/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech
 ___ _____ _   ___ _  _____ ___  ___  ___ ___
/ __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
\__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
|___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
embedded.connectivity.solutions===============

Description: LoRa MAC region AU915 implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis ( Semtech ), Gregory Cristian ( Semtech ) and Daniel Jaeckle ( STACKFORCE )
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "radio.h"
#include "timer.h"
#include "utilities.h"

#include "LoRaMac.h"
#include "Region.h"
#include "RegionCommon.h"
#include "RegionAU915.h"

// Definitions
#define CHANNELS_MASK_SIZE              6

// Global attributes
/*!
 * LoRaMAC channels
 */
extern ChannelParams_t LoRaMacChannels[AU915_MAX_NB_CHANNELS];

/*!
 * LoRaMac bands
 */
extern Band_t LoRaMacBands[AU915_MAX_NB_BANDS];

/*!
 * LoRaMac channels mask
 */
extern uint16_t LoRaMacChannelsMask[CHANNELS_MASK_SIZE];

/*!
 * LoRaMac channels remaining
 */
extern uint16_t LoRaMacChannelsMaskRemaining[CHANNELS_MASK_SIZE];

/*!
 * LoRaMac channels default mask
 */
extern uint16_t LoRaMacChannelsDefaultMask[CHANNELS_MASK_SIZE];

// Static functions
static int8_t GetNextLowerTxDr( int8_t dr, int8_t minDr )
{
    uint8_t nextLowerDr = 0;

    if( dr == minDr )
    {
        nextLowerDr = minDr;
    }
    else
    {
        nextLowerDr = dr - 1;
    }
    return nextLowerDr;
}

static uint32_t GetBandwidth( uint32_t drIndex )
{
    switch( BandwidthsAU915[drIndex] )
    {
        default:
        case 125000:
            return 0;
        case 250000:
            return 1;
        case 500000:
            return 2;
    }
}

static int8_t LimitTxPower( int8_t txPower, int8_t maxBandTxPower, int8_t datarate, uint16_t* channelsMask )
{
    int8_t txPowerResult = txPower;

    // Limit tx power to the band max
    txPowerResult =  MAX( txPower, maxBandTxPower );

    return txPowerResult;
}

static uint8_t CountNbOfEnabledChannels( uint8_t datarate, uint16_t* channelsMask, ChannelParams_t* channels, Band_t* bands, uint8_t* enabledChannels, uint8_t* delayTx )
{
    uint8_t nbEnabledChannels = 0;
    uint8_t delayTransmission = 0;

    for( uint8_t i = 0, k = 0; i < AU915_MAX_NB_CHANNELS; i += 16, k++ )
    {
        for( uint8_t j = 0; j < 16; j++ )
        {
            if( ( channelsMask[k] & ( 1 << j ) ) != 0 )
            {
                if( channels[i + j].Frequency == 0 )
                { // Check if the channel is enabled
                    continue;
                }
                if( RegionCommonValueInRange( datarate, channels[i + j].DrRange.Fields.Min,
                                              channels[i + j].DrRange.Fields.Max ) == false )
                { // Check if the current channel selection supports the given datarate
                    continue;
                }
                if( bands[channels[i + j].Band].TimeOff > 0 )
                { // Check if the band is available for transmission
                    delayTransmission++;
                    continue;
                }
                enabledChannels[nbEnabledChannels++] = i + j;
            }
        }
    }

    *delayTx = delayTransmission;
    return nbEnabledChannels;
}

static PhyParam_t RegionAU915GetPhyParam( GetPhyParams_t* getPhy )
{
    PhyParam_t phyParam = { 0 };

    switch( getPhy->Attribute )
    {
        case PHY_MIN_RX_DR:
        {
            phyParam.Value = AU915_RX_MIN_DATARATE;
            break;
        }
        case PHY_MIN_TX_DR:
        {
            phyParam.Value = AU915_TX_MIN_DATARATE;
            break;
        }
        case PHY_DEF_TX_DR:
        {
            phyParam.Value = AU915_DEFAULT_DATARATE;
            break;
        }
        case PHY_NEXT_LOWER_TX_DR:
        {
            phyParam.Value = GetNextLowerTxDr( getPhy->Datarate, AU915_TX_MIN_DATARATE );
            break;
        }
        case PHY_DEF_TX_POWER:
        {
            phyParam.Value = AU915_DEFAULT_TX_POWER;
            break;
        }
        case PHY_MAX_PAYLOAD:
        {
            phyParam.Value = MaxPayloadOfDatarateAU915[getPhy->Datarate];
            break;
        }
        case PHY_MAX_PAYLOAD_REPEATER:
        {
            phyParam.Value = MaxPayloadOfDatarateRepeaterAU915[getPhy->Datarate];
            break;
        }
        case PHY_DUTY_CYCLE:
        {
            phyParam.Value = AU915_DUTY_CYCLE_ENABLED;
            break;
        }
        case PHY_MAX_RX_WINDOW:
        {
            phyParam.Value = AU915_MAX_RX_WINDOW;
            break;
        }
        case PHY_RECEIVE_DELAY1:
        {
            phyParam.Value = AU915_RECEIVE_DELAY1;
            break;
        }
        case PHY_RECEIVE_DELAY2:
        {
            phyParam.Value = AU915_RECEIVE_DELAY2;
            break;
        }
        case PHY_JOIN_ACCEPT_DELAY1:
        {
            phyParam.Value = AU915_JOIN_ACCEPT_DELAY1;
            break;
        }
        case PHY_JOIN_ACCEPT_DELAY2:
        {
            phyParam.Value = AU915_JOIN_ACCEPT_DELAY2;
            break;
        }
        case PHY_MAX_FCNT_GAP:
        {
            phyParam.Value = AU915_MAX_FCNT_GAP;
            break;
        }
        case PHY_ACK_TIMEOUT:
        {
            phyParam.Value = ( AU915_ACKTIMEOUT + randr( -AU915_ACK_TIMEOUT_RND, AU915_ACK_TIMEOUT_RND ) );
            break;
        }
        case PHY_DEF_DR1_OFFSET:
        {
            phyParam.Value = AU915_DEFAULT_RX1_DR_OFFSET;
            break;
        }
        case PHY_DEF_RX2_FREQUENCY:
        {
            phyParam.Value = AU915_RX_WND_2_FREQ;
            break;
        }
        case PHY_DEF_RX2_DR:
        {
            phyParam.Value = AU915_RX_WND_2_DR;
            break;
        }
        case PHY_CHANNELS_MASK:
        {
            phyParam.ChannelsMask = LoRaMacChannelsMask;
            break;
        }
        case PHY_CHANNELS_DEFAULT_MASK:
        {
            phyParam.ChannelsMask = LoRaMacChannelsDefaultMask;
            break;
        }
        case PHY_MAX_NB_CHANNELS:
        {
            phyParam.Value = AU915_MAX_NB_CHANNELS;
            break;
        }
        case PHY_CHANNELS:
        {
            phyParam.Channels = LoRaMacChannels;
            break;
        }
        case PHY_DEF_UPLINK_DWELL_TIME:
        case PHY_DEF_DOWNLINK_DWELL_TIME:
        {
            phyParam.Value = 0;
            break;
        }
        case PHY_DEF_MAX_EIRP:
        {
            phyParam.fValue = AU915_DEFAULT_MAX_EIRP;
            break;
        }
        case PHY_DEF_ANTENNA_GAIN:
        {
            phyParam.fValue = AU915_DEFAULT_ANTENNA_GAIN;
            break;
        }
        case PHY_NB_JOIN_TRIALS:
        case PHY_DEF_NB_JOIN_TRIALS:
        {
            phyParam.Value = 2;
            break;
        }
        default:
        {
            break;
        }
    }

    return phyParam;
}

static void RegionAU915SetBandTxDone( SetBandTxDoneParams_t* txDone )
{
    RegionCommonSetBandTxDone( txDone->Joined, &LoRaMacBands[LoRaMacChannels[txDone->Channel].Band], txDone->LastTxDoneTime );
}

static void RegionAU915InitDefaults( InitType_t type )
{
    switch( type )
    {
        case INIT_TYPE_INIT:
        {
	    // Bands
	    LoRaMacBands[0] = ( Band_t ) AU915_BAND0;

            // Channels
            // 125 kHz channels
            for( uint8_t i = 0; i < AU915_MAX_NB_CHANNELS - 8; i++ )
            {
                LoRaMacChannels[i].Frequency = 915200000 + i * 200000;
                LoRaMacChannels[i].DrRange.Value = ( DR_5 << 4 ) | DR_0;
                LoRaMacChannels[i].Band = 0;
            }
            // 500 kHz channels
            for( uint8_t i = AU915_MAX_NB_CHANNELS - 8; i < AU915_MAX_NB_CHANNELS; i++ )
            {
                LoRaMacChannels[i].Frequency = 915900000 + ( i - ( AU915_MAX_NB_CHANNELS - 8 ) ) * 1600000;
                LoRaMacChannels[i].DrRange.Value = ( DR_6 << 4 ) | DR_6;
                LoRaMacChannels[i].Band = 0;
            }

            // Initialize channels default mask
            LoRaMacChannelsDefaultMask[0] = 0xFFFF;
            LoRaMacChannelsDefaultMask[1] = 0xFFFF;
            LoRaMacChannelsDefaultMask[2] = 0xFFFF;
            LoRaMacChannelsDefaultMask[3] = 0xFFFF;
            LoRaMacChannelsDefaultMask[4] = 0x00FF;
            LoRaMacChannelsDefaultMask[5] = 0x0000;

            // Copy channels default mask
            RegionCommonChanMaskCopy( LoRaMacChannelsMask, LoRaMacChannelsDefaultMask, 6 );

            // Copy into channels mask remaining
            RegionCommonChanMaskCopy( LoRaMacChannelsMaskRemaining, LoRaMacChannelsMask, 6 );
            break;
        }
        case INIT_TYPE_RESTORE:
        {
            // Copy channels default mask
            RegionCommonChanMaskCopy( LoRaMacChannelsMask, LoRaMacChannelsDefaultMask, 6 );

            for( uint8_t i = 0; i < 6; i++ )
            { // Copy-And the channels mask
                LoRaMacChannelsMaskRemaining[i] &= LoRaMacChannelsMask[i];
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

static bool RegionAU915Verify( VerifyParams_t* verify, PhyAttribute_t phyAttribute )
{
    switch( phyAttribute )
    {
        case PHY_TX_DR:
        case PHY_DEF_TX_DR:
        {
            return RegionCommonValueInRange( verify->DatarateParams.Datarate, AU915_TX_MIN_DATARATE, AU915_TX_MAX_DATARATE );
        }
        case PHY_RX_DR:
        {
            return RegionCommonValueInRange( verify->DatarateParams.Datarate, AU915_RX_MIN_DATARATE, AU915_RX_MAX_DATARATE );
        }
        case PHY_DEF_TX_POWER:
        case PHY_TX_POWER:
        {
            // Remark: switched min and max!
            return RegionCommonValueInRange( verify->TxPower, AU915_MAX_TX_POWER, AU915_MIN_TX_POWER );
        }
        case PHY_DUTY_CYCLE:
        {
            return AU915_DUTY_CYCLE_ENABLED;
        }
        case PHY_NB_JOIN_TRIALS:
        {
            if( verify->NbJoinTrials < 2 )
            {
                return false;
            }
            break;
        }
        default:
            return false;
    }
    return true;
}

static void RegionAU915ApplyCFList( ApplyCFListParams_t* applyCFList )
{
    return;
}

static bool RegionAU915ChanMaskSet( ChanMaskSetParams_t* chanMaskSet )
{
    uint8_t nbChannels = RegionCommonCountChannels( chanMaskSet->ChannelsMaskIn, 0, 4 );

    // Check the number of active channels
    // According to ACMA regulation, we require at least 20 125KHz channels, if
    // the node shall utilize 125KHz channels.
    if( ( nbChannels < 20 ) &&
        ( nbChannels > 0 ) )
    {
        return false;
    }

    switch( chanMaskSet->ChannelsMaskType )
    {
        case CHANNELS_MASK:
        {
            for( uint8_t i = 0; i < 6; i++ )
            {
		if (chanMaskSet->ChannelsMaskIn[i] & ~LoRaMacChannelsDefaultMask[i])
		{
		    return false;
		}
            }

            RegionCommonChanMaskCopy( LoRaMacChannelsMask, chanMaskSet->ChannelsMaskIn, 6 );

            for( uint8_t i = 0; i < 6; i++ )
            { // Copy-And the channels mask
                LoRaMacChannelsMaskRemaining[i] &= LoRaMacChannelsMask[i];
            }
            break;
        }
        case CHANNELS_DEFAULT_MASK:
        {
            RegionCommonChanMaskCopy( LoRaMacChannelsDefaultMask, chanMaskSet->ChannelsMaskIn, 6 );
            break;
        }
        default:
            return false;
    }
    return true;
}

static bool RegionAU915AdrNext( AdrNextParams_t* adrNext, int8_t* drOut, int8_t* txPowOut, uint32_t* adrAckCounter )
{
    bool adrAckReq = false;
    int8_t datarate = adrNext->Datarate;
    int8_t txPower = adrNext->TxPower;
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;

    // Report back the adr ack counter
    *adrAckCounter = adrNext->AdrAckCounter;

    if( adrNext->AdrEnabled == true )
    {
        if( datarate == AU915_TX_MIN_DATARATE )
        {
            *adrAckCounter = 0;
            adrAckReq = false;
        }
        else
        {
            if( adrNext->AdrAckCounter >= AU915_ADR_ACK_LIMIT )
            {
                adrAckReq = true;
                txPower = AU915_MAX_TX_POWER;
            }
            else
            {
                adrAckReq = false;
            }
            if( adrNext->AdrAckCounter >= ( AU915_ADR_ACK_LIMIT + AU915_ADR_ACK_DELAY ) )
            {
                if( ( adrNext->AdrAckCounter % AU915_ADR_ACK_DELAY ) == 1 )
                {
                    // Decrease the datarate
                    getPhy.Attribute = PHY_NEXT_LOWER_TX_DR;
                    getPhy.Datarate = datarate;
                    getPhy.UplinkDwellTime = adrNext->UplinkDwellTime;
                    phyParam = RegionAU915GetPhyParam( &getPhy );
                    datarate = phyParam.Value;

                    if( datarate == AU915_TX_MIN_DATARATE )
                    {
                        // We must set adrAckReq to false as soon as we reach the lowest datarate
                        adrAckReq = false;
                        if( adrNext->UpdateChanMask == true )
                        {
                            // Re-enable default channels
			    LoRaMacChannelsMask[0] = LoRaMacChannelsDefaultMask[0];
			    LoRaMacChannelsMask[1] = LoRaMacChannelsDefaultMask[1];
                            LoRaMacChannelsMask[2] = LoRaMacChannelsDefaultMask[2];
                            LoRaMacChannelsMask[3] = LoRaMacChannelsDefaultMask[3];
                            LoRaMacChannelsMask[4] = LoRaMacChannelsDefaultMask[4];
                            LoRaMacChannelsMask[5] = LoRaMacChannelsDefaultMask[5];
                        }
                    }
                }
            }
        }
    }

    *drOut = datarate;
    *txPowOut = txPower;
    return adrAckReq;
}

static void RegionAU915ComputeRxWindowParameters( int8_t datarate, uint8_t minRxSymbols, uint32_t rxError, RxConfigParams_t *rxConfigParams )
{
    double tSymbol = 0.0;

    // Get the datarate, perform a boundary check
    rxConfigParams->Datarate = MIN( datarate, AU915_RX_MAX_DATARATE );
    rxConfigParams->Bandwidth = GetBandwidth( rxConfigParams->Datarate );

    tSymbol = RegionCommonComputeSymbolTimeLoRa( DataratesAU915[rxConfigParams->Datarate], BandwidthsAU915[rxConfigParams->Datarate] );

    RegionCommonComputeRxWindowParameters( tSymbol, minRxSymbols, rxError, Radio.GetRadioWakeUpTime(), &rxConfigParams->WindowTimeout, &rxConfigParams->WindowOffset );
}

static bool RegionAU915RxConfig( RxConfigParams_t* rxConfig, int8_t* datarate )
{
    int8_t dr = rxConfig->Datarate;
    uint8_t maxPayload = 0;
    int8_t phyDr = 0;
    uint32_t frequency = rxConfig->Frequency;

    if( Radio.GetStatus( ) != RF_IDLE )
    {
        return false;
    }

    if( rxConfig->Window == 0 )
    {
        // Apply window 1 frequency
        frequency = AU915_FIRST_RX1_CHANNEL + ( rxConfig->Channel % 8 ) * AU915_STEPWIDTH_RX1_CHANNEL;
    }

    // Read the physical datarate from the datarates table
    phyDr = DataratesAU915[dr];

    Radio.SetChannel( frequency );

    // Radio configuration
    Radio.SetRxConfig( MODEM_LORA, rxConfig->Bandwidth, phyDr, 1, 0, 8, rxConfig->WindowTimeout, false, 0, false, 0, 0, true, rxConfig->RxContinuous );

    if( rxConfig->RepeaterSupport == true )
    {
        maxPayload = MaxPayloadOfDatarateRepeaterAU915[dr];
    }
    else
    {
        maxPayload = MaxPayloadOfDatarateAU915[dr];
    }
    Radio.SetMaxPayloadLength( MODEM_LORA, maxPayload + LORA_MAC_FRMPAYLOAD_OVERHEAD );

    *datarate = (uint8_t) dr;
    return true;
}

static bool RegionAU915TxConfig( TxConfigParams_t* txConfig, int8_t* txPower, TimerTime_t* txTimeOnAir )
{
    int8_t phyDr = DataratesAU915[txConfig->Datarate];
    int8_t txPowerLimited = LimitTxPower( txConfig->TxPower, LoRaMacBands[LoRaMacChannels[txConfig->Channel].Band].TxMaxPower, txConfig->Datarate, LoRaMacChannelsMask );
    uint32_t bandwidth = GetBandwidth( txConfig->Datarate );
    int8_t phyTxPower = 0;

    // Calculate physical TX power
    phyTxPower = RegionCommonComputeTxPower( txPowerLimited, txConfig->MaxEirp, txConfig->AntennaGain );

    // Setup the radio frequency
    Radio.SetChannel( LoRaMacChannels[txConfig->Channel].Frequency );

    Radio.SetTxConfig( MODEM_LORA, phyTxPower, 0, bandwidth, phyDr, 1, 8, false, true, 0, 0, false, 3000 );

    // Setup maximum payload lenght of the radio driver
    Radio.SetMaxPayloadLength( MODEM_LORA, txConfig->PktLen );

    *txTimeOnAir = Radio.TimeOnAir( MODEM_LORA, txConfig->PktLen );
    *txPower = txPowerLimited;

    return true;
}

static uint8_t RegionAU915LinkAdrReq( LinkAdrReqParams_t* linkAdrReq, int8_t* drOut, int8_t* txPowOut, uint8_t* nbRepOut, uint8_t* nbBytesParsed )
{
    uint8_t status = 0x07;
    RegionCommonLinkAdrParams_t linkAdrParams;
    uint8_t nextIndex = 0;
    uint8_t bytesProcessed = 0;
    uint16_t channelsMask[6] = { 0, 0, 0, 0, 0, 0 };
    GetPhyParams_t getPhy;
    PhyParam_t phyParam;
    RegionCommonLinkAdrReqVerifyParams_t linkAdrVerifyParams;

    // Initialize local copy of channels mask
    RegionCommonChanMaskCopy( channelsMask, LoRaMacChannelsMask, 6 );

    while( bytesProcessed < linkAdrReq->PayloadSize )
    {
        nextIndex = RegionCommonParseLinkAdrReq( &( linkAdrReq->Payload[bytesProcessed] ), &linkAdrParams );

        if( nextIndex == 0 )
            break; // break loop, since no more request has been found

        // Update bytes processed
        bytesProcessed += nextIndex;

        // Revert status, as we only check the last ADR request for the channel mask KO
        status = 0x07;

        if( linkAdrParams.ChMaskCtrl == 6 )
        {
            // Enable all 125 kHz channels
            channelsMask[0] = 0xFFFF;
            channelsMask[1] = 0xFFFF;
            channelsMask[2] = 0xFFFF;
            channelsMask[3] = 0xFFFF;
            // Apply chMask to channels 64 to 71
            channelsMask[4] = linkAdrParams.ChMask;
        }
        else if( linkAdrParams.ChMaskCtrl == 7 )
        {
            // Disable all 125 kHz channels
            channelsMask[0] = 0x0000;
            channelsMask[1] = 0x0000;
            channelsMask[2] = 0x0000;
            channelsMask[3] = 0x0000;
            // Apply chMask to channels 64 to 71
            channelsMask[4] = linkAdrParams.ChMask;
        }
        else if( linkAdrParams.ChMaskCtrl == 5 )
        {
	    // chMask selects [chMask * 8, chMask * 8 +7],64+chMask
            channelsMask[0] = 0x0000;
            channelsMask[1] = 0x0000;
            channelsMask[2] = 0x0000;
            channelsMask[3] = 0x0000;
	    channelsMask[(linkAdrParams.ChMask & 7) >> 1] = ((linkAdrParams.ChMask & 1) ? 0xFF00 : 0x00FF);
	    channelsMask[4] = (0x0001 << (linkAdrParams.ChMask & 7));
        }
        else
        {
            channelsMask[linkAdrParams.ChMaskCtrl] = linkAdrParams.ChMask;
        }
    }

    // FCC 15.247 paragraph F mandates to hop on at least 2 125 kHz channels
    if( ( linkAdrParams.Datarate < DR_6 ) && ( RegionCommonCountChannels( channelsMask, 0, 4 ) < 2 ) )
    {
        status &= 0xFE; // Channel mask KO
    }

    for( uint8_t i = 0; i < 6; i++ )
    {
	if (channelsMask[i] & ~LoRaMacChannelsDefaultMask[i])
	{
	    status &= 0xFE; // Channel mask KO
	}
    }

    // Get the minimum possible datarate
    getPhy.Attribute = PHY_MIN_TX_DR;
    getPhy.UplinkDwellTime = linkAdrReq->UplinkDwellTime;
    phyParam = RegionAU915GetPhyParam( &getPhy );

    linkAdrVerifyParams.Status = status;
    linkAdrVerifyParams.AdrEnabled = linkAdrReq->AdrEnabled;
    linkAdrVerifyParams.Datarate = linkAdrParams.Datarate;
    linkAdrVerifyParams.TxPower = linkAdrParams.TxPower;
    linkAdrVerifyParams.NbRep = linkAdrParams.NbRep;
    linkAdrVerifyParams.CurrentDatarate = linkAdrReq->CurrentDatarate;
    linkAdrVerifyParams.CurrentTxPower = linkAdrReq->CurrentTxPower;
    linkAdrVerifyParams.CurrentNbRep = linkAdrReq->CurrentNbRep;
    linkAdrVerifyParams.NbChannels = AU915_MAX_NB_CHANNELS;
    linkAdrVerifyParams.ChannelsMask = channelsMask;
    linkAdrVerifyParams.MinDatarate = ( int8_t )phyParam.Value;
    linkAdrVerifyParams.MaxDatarate = AU915_TX_MAX_DATARATE;
    linkAdrVerifyParams.Channels = LoRaMacChannels;
    linkAdrVerifyParams.MinTxPower = AU915_MIN_TX_POWER;
    linkAdrVerifyParams.MaxTxPower = AU915_MAX_TX_POWER;

    // Verify the parameters and update, if necessary
    status = RegionCommonLinkAdrReqVerifyParams( &linkAdrVerifyParams, &linkAdrParams.Datarate, &linkAdrParams.TxPower, &linkAdrParams.NbRep );

    // Update channelsMask if everything is correct
    if( status == 0x07 )
    {
        // Copy Mask
        RegionCommonChanMaskCopy( LoRaMacChannelsMask, channelsMask, 6 );

        LoRaMacChannelsMaskRemaining[0] &= LoRaMacChannelsMask[0];
        LoRaMacChannelsMaskRemaining[1] &= LoRaMacChannelsMask[1];
        LoRaMacChannelsMaskRemaining[2] &= LoRaMacChannelsMask[2];
        LoRaMacChannelsMaskRemaining[3] &= LoRaMacChannelsMask[3];
        LoRaMacChannelsMaskRemaining[4] = LoRaMacChannelsMask[4];
        LoRaMacChannelsMaskRemaining[5] = LoRaMacChannelsMask[5];
    }

    // Update status variables
    *drOut = linkAdrParams.Datarate;
    *txPowOut = linkAdrParams.TxPower;
    *nbRepOut = linkAdrParams.NbRep;
    *nbBytesParsed = bytesProcessed;

    return status;
}

static uint8_t RegionAU915RxParamSetupReq( RxParamSetupReqParams_t* rxParamSetupReq )
{
    uint8_t status = 0x07;
    uint32_t freq = rxParamSetupReq->Frequency;

    // Verify radio frequency
    if( ( Radio.CheckRfFrequency( freq ) == false ) ||
        ( freq < AU915_FIRST_RX1_CHANNEL ) ||
        ( freq > AU915_LAST_RX1_CHANNEL ) ||
        ( ( ( freq - ( uint32_t ) AU915_FIRST_RX1_CHANNEL ) % ( uint32_t ) AU915_STEPWIDTH_RX1_CHANNEL ) != 0 ) )
    {
        status &= 0xFE; // Channel frequency KO
    }

    // Verify datarate
    if( RegionCommonValueInRange( rxParamSetupReq->Datarate, AU915_RX_MIN_DATARATE, AU915_RX_MAX_DATARATE ) == false )
    {
        status &= 0xFD; // Datarate KO
    }
    if( ( rxParamSetupReq->Datarate == DR_7 ) ||
        ( rxParamSetupReq->Datarate > DR_13 ) )
    {
        status &= 0xFD; // Datarate KO
    }

    // Verify datarate offset
    if( RegionCommonValueInRange( rxParamSetupReq->DrOffset, AU915_MIN_RX1_DR_OFFSET, AU915_MAX_RX1_DR_OFFSET ) == false )
    {
        status &= 0xFB; // Rx1DrOffset range KO
    }

    return status;
}

static uint8_t RegionAU915NewChannelReq( NewChannelReqParams_t* newChannelReq )
{
    // Datarate and frequency KO
    return 0;
}

static int8_t RegionAU915TxParamSetupReq( TxParamSetupReqParams_t* txParamSetupReq )
{
    return -1;
}

static uint8_t RegionAU915DlChannelReq( DlChannelReqParams_t* dlChannelReq )
{
    return 0;
}

static int8_t RegionAU915AlternateDr( AlternateDrParams_t* alternateDr )
{
    int8_t datarate = 0;

    // Re-enable 500 kHz default channels
    LoRaMacChannelsMask[4] = LoRaMacChannelsDefaultMask[4];

    if( ( alternateDr->NbTrials & 0x01 ) == 0x01 )
    {
        datarate = DR_6;
    }
    else
    {
        datarate = DR_0;
    }
    return datarate;
}

static void RegionAU915CalcBackOff( CalcBackOffParams_t* calcBackOff )
{
    RegionCommonCalcBackOffParams_t calcBackOffParams;

    calcBackOffParams.Channels = LoRaMacChannels;
    calcBackOffParams.Bands = LoRaMacBands;
    calcBackOffParams.LastTxIsJoinRequest = calcBackOff->LastTxIsJoinRequest;
    calcBackOffParams.Joined = calcBackOff->Joined;
    calcBackOffParams.DutyCycleEnabled = calcBackOff->DutyCycleEnabled;
    calcBackOffParams.Channel = calcBackOff->Channel;
    calcBackOffParams.ElapsedTime = calcBackOff->ElapsedTime;
    calcBackOffParams.TxTimeOnAir = calcBackOff->TxTimeOnAir;

    RegionCommonCalcBackOff( &calcBackOffParams );
}

static bool RegionAU915NextChannel( NextChanParams_t* nextChanParams, uint8_t* channel, TimerTime_t* time, TimerTime_t* aggregatedTimeOff )
{
    uint8_t nbEnabledChannels = 0;
    uint8_t delayTx = 0;
    uint8_t enabledChannels[AU915_MAX_NB_CHANNELS] = { 0 };
    TimerTime_t nextTxDelay = 0;

    // Count 125kHz channels
    if( RegionCommonCountChannels( LoRaMacChannelsMaskRemaining, 0, 4 ) == 0 )
    { // Reactivate default channels
        RegionCommonChanMaskCopy( LoRaMacChannelsMaskRemaining, LoRaMacChannelsMask, 4  );
    }
    // Check other channels
    if( nextChanParams->Datarate >= DR_6 )
    {
        if( ( LoRaMacChannelsMaskRemaining[4] & 0x00FF ) == 0 )
        {
            LoRaMacChannelsMaskRemaining[4] = LoRaMacChannelsMask[4];
        }
    }

    if( nextChanParams->AggrTimeOff <= TimerGetElapsedTime( nextChanParams->LastAggrTx ) )
    {
        // Reset Aggregated time off
        *aggregatedTimeOff = 0;

        // Update bands Time OFF
        nextTxDelay = RegionCommonUpdateBandTimeOff( nextChanParams->Joined, nextChanParams->DutyCycleEnabled, LoRaMacBands, AU915_MAX_NB_BANDS );

        // Search how many channels are enabled
        nbEnabledChannels = CountNbOfEnabledChannels( nextChanParams->Datarate,
                                                      LoRaMacChannelsMaskRemaining, LoRaMacChannels,
                                                      LoRaMacBands, enabledChannels, &delayTx );
    }
    else
    {
        delayTx++;
        nextTxDelay = nextChanParams->AggrTimeOff - TimerGetElapsedTime( nextChanParams->LastAggrTx );
    }

    if( nbEnabledChannels > 0 )
    {
        // We found a valid channel
        *channel = enabledChannels[randr( 0, nbEnabledChannels - 1 )];
        // Disable the channel in the mask
        RegionCommonChanDisable( LoRaMacChannelsMaskRemaining, *channel, AU915_MAX_NB_CHANNELS - 8 );

        *time = 0;
        return true;
    }
    else
    {
        if( delayTx > 0 )
        {
            // Delay transmission due to AggregatedTimeOff or to a band time off
            *time = nextTxDelay;
            return true;
        }
        // Datarate not supported by any channel
        *time = 0;
        return false;
    }
}

static LoRaMacStatus_t RegionAU915ChannelAdd( ChannelAddParams_t* channelAdd )
{
    return LORAMAC_STATUS_PARAMETER_INVALID;
}

static bool RegionAU915ChannelsRemove( ChannelRemoveParams_t* channelRemove  )
{
    return LORAMAC_STATUS_PARAMETER_INVALID;
}

static void RegionAU915SetContinuousWave( ContinuousWaveParams_t* continuousWave )
{
    int8_t txPowerLimited = LimitTxPower( continuousWave->TxPower, LoRaMacBands[LoRaMacChannels[continuousWave->Channel].Band].TxMaxPower, continuousWave->Datarate, LoRaMacChannelsMask );
    int8_t phyTxPower = 0;
    uint32_t frequency = LoRaMacChannels[continuousWave->Channel].Frequency;

    // Calculate physical TX power
    phyTxPower = RegionCommonComputeTxPower( txPowerLimited, continuousWave->MaxEirp, continuousWave->AntennaGain );

    Radio.SetTxContinuousWave( frequency, phyTxPower, continuousWave->Timeout );
}

static uint8_t RegionAU915ApplyDrOffset( uint8_t downlinkDwellTime, int8_t dr, int8_t drOffset )
{
    int8_t datarate = DatarateOffsetsAU915[dr][drOffset];

    if( datarate < 0 )
    {
        datarate = DR_0;
    }
    return datarate;
}

const LoRaMacRegion_t LoRaMacRegionAU915 = {
    RegionAU915GetPhyParam,
    RegionAU915SetBandTxDone,
    RegionAU915InitDefaults,
    RegionAU915Verify,
    RegionAU915ApplyCFList,
    RegionAU915ChanMaskSet,
    RegionAU915AdrNext,
    RegionAU915ComputeRxWindowParameters,
    RegionAU915RxConfig,
    RegionAU915TxConfig,
    RegionAU915LinkAdrReq,
    RegionAU915RxParamSetupReq,
    RegionAU915NewChannelReq,
    RegionAU915TxParamSetupReq,
    RegionAU915DlChannelReq,
    RegionAU915AlternateDr,
    RegionAU915CalcBackOff,
    RegionAU915NextChannel,
    RegionAU915ChannelAdd,
    RegionAU915ChannelsRemove,
    RegionAU915SetContinuousWave,
    RegionAU915ApplyDrOffset,
};
