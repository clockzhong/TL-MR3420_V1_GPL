var str_wps_name_long = "Quick Secure Setup";
var str_wps_name_short = "QSS";
var our_web_site = "www.tp-link.com";
var wireless_ssid_prefix = "TP-LINK";
var operModeNum = 9;
var minOperMode = 0;
var maxOperMode = operModeNum;
var operModeList = new Array(
//value	enabled	name
	0,		1,			"Access Point",
	1,		1,			"Multi-SSID",
	2,		0,			"Multi-Bss Plus VLAN",
	3,		1,			"Client",
	4,		1,			"Repeater",
	5,		1,			"Universal Repeater",
	6,		0,			"Bridge",
	7,		1,			"Bridge with AP",
	8,		0,			"Debug"
);
function getOperModeName(modeIdx)
{
	if(modeIdx<minOperMode || modeIdx>maxOperMode)
	{
		return null;
	}
	if(operModeList[modeIdx*3+1]==0)
	{
		return null;
	}
	else
	{
		return operModeList[modeIdx*3+2];
	}
}

function operModeEnable(modeIdx)
{
	if(modeIdx<minOperMode || modeIdx>maxOperMode)
	{
		return null;
	}
	if(operModeList[modeIdx*3+1]==0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

function getOperModeValue(modeIdx)
{
	if(modeIdx<minOperMode || modeIdx>maxOperMode)
	{
		return null;
	}
	if(operModeList[modeIdx*3+1]==0)
	{
		return null;
	}
	else
	{
		return operModeList[modeIdx*3];
	}
}
function getOperModeIdxByValue(modeValue)
{
	for(var i=0; i<operModeNum; i++)
	{
		if(operModeList[i*3] == modeValue)
			return i;
	}
	return null;
}