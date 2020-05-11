/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#include "scopeprotocols.h"
#include "HorizontalBathtubDecoder.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

HorizontalBathtubDecoder::HorizontalBathtubDecoder(string color)
	: ProtocolDecoder(OscilloscopeChannel::CHANNEL_TYPE_ANALOG, color, CAT_ANALYSIS)
{
	m_yAxisUnit = Unit(Unit::UNIT_COUNTS);

	//Set up channels
	m_signalNames.push_back("din");
	m_channels.push_back(NULL);

	m_midpoint = 0.5;
	m_range = 1;

	m_voltageName = "Voltage";
	m_parameters[m_voltageName] = ProtocolDecoderParameter(ProtocolDecoderParameter::TYPE_FLOAT);
	m_parameters[m_voltageName].SetFloatVal(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

bool HorizontalBathtubDecoder::ValidateChannel(size_t i, OscilloscopeChannel* channel)
{
	if( (i == 0) && (dynamic_cast<EyeDecoder2*>(channel) != NULL) )
		return true;
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

void HorizontalBathtubDecoder::SetDefaultName()
{
	char hwname[256];
	snprintf(hwname, sizeof(hwname), "HBathtub(%s, %.2f)",
		m_channels[0]->m_displayname.c_str(),
		m_parameters[m_voltageName].GetFloatVal()
		);
	m_hwname = hwname;
	m_displayname = m_hwname;
}

string HorizontalBathtubDecoder::GetProtocolName()
{
	return "Horz Bathtub";
}

bool HorizontalBathtubDecoder::IsOverlay()
{
	//we create a new analog channel
	return false;
}

bool HorizontalBathtubDecoder::NeedsConfig()
{
	return true;
}

double HorizontalBathtubDecoder::GetVoltageRange()
{
	return m_range;
}

double HorizontalBathtubDecoder::GetOffset()
{
	return -m_midpoint;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

void HorizontalBathtubDecoder::Refresh()
{
	//Get the input data
	if(m_channels[0] == NULL)
	{
		SetData(NULL);
		return;
	}
	EyeCapture2* din = dynamic_cast<EyeCapture2*>(m_channels[0]->GetData());
	if(din == NULL)
	{
		SetData(NULL);
		return;
	}

	float threshold = m_parameters[m_voltageName].GetFloatVal();

	//Find the eye bin for this height
	float yscale = din->GetHeight() / m_channels[0]->GetVoltageRange();
	float ymid = din->GetHeight()/2;
	float center = din->GetCenterVoltage();

	//Sanity check we're not off the eye
	size_t ybin = round( (threshold-center)*yscale + ymid);
	if(ybin >= din->GetHeight())
		return;

	//Horizontal scale: one eye is two UIs wide
	double ps_per_width = 2*din->m_uiWidth;
	double ps_per_pixel = ps_per_width / din->GetWidth();

	//Create the output
	AnalogCapture* cap = new AnalogCapture;

	//Extract the single scanline we're interested in
	int64_t* row = din->GetAccumData() + ybin*din->GetWidth();
	m_range = 0;
	for(size_t i=0; i<din->GetWidth(); i++)
	{
		int64_t sample = row[i];
		if(sample > m_range)
			m_range = sample;

		cap->m_samples.push_back(AnalogSample(i*ps_per_pixel - din->m_uiWidth, ps_per_pixel, sample));
	}

	//Scale so we don't quite touch the edges of the plot
	m_range *= 1.05;
	m_midpoint = m_range/2;

	SetData(cap);

	//Copy start time etc from the input. Timestamps are in picoseconds.
	cap->m_timescale = din->m_timescale;
	cap->m_startTimestamp = din->m_startTimestamp;
	cap->m_startPicoseconds = din->m_startPicoseconds;
}
