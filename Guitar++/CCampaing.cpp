#include "CCampaing.h"

CCampaing::CCampaingData::CCampaingData()
{

}

CCampaing &CCampaing::campaingMGR()
{
	static CCampaing campaing;
	return campaing;
}

CCampaing::CCampaing()
{

}
