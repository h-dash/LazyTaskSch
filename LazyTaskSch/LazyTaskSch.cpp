// LazyTaskSch.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

#include <string>
#include <vector>
#include <time.h>

#ifdef _UNICODE
typedef std::wstring	tstring;
#else
typedef std::string		tstring;
#endif

class CTaskKind {

public:
	CTaskKind (){};
	~CTaskKind (){};

protected:

	unsigned long	_dwTaskKindID;
	tstring			_strTaskKindName;	

};
typedef std::vector<CTaskKind>	TASKKIND_VECTOR;

class CTimeSec {

public:

	CTimeSec(){}

	CTimeSec (int nY,int nM,int nD,int nH = 0,int nI = 0,int nS = 0){
		SetTime(nY,nM,nD,nH,nI,nS);
	}
	~CTimeSec (){}

	void SetTime(int nY,int nM,int nD,int nH = 0,int nI = 0,int nS = 0){

		struct tm tmTmp;

		tmTmp.tm_year	= nY - 1900;
		tmTmp.tm_mon	= nM;
		tmTmp.tm_mday	= nD;
		tmTmp.tm_hour	= nH;
		tmTmp.tm_min	= nI;
		tmTmp.tm_sec	= nS;

		_tmt = mktime(&tmTmp);
	}

	void AddDay(int nDay)		{	AddHour(24);				}
	void AddHour(int nHour)		{	AddMinute(60 * nHour);		}
	void AddMinute(int nMinute)	{	AddSecond(60 * nMinute);	}
	void AddSecond(int nSecond)	{	_tmt += nSecond;			}

	time_t GetTime(){	return _tmt;	}

protected:

	time_t	_tmt;

};

class CTimeRange {

public:
	CTimeRange(){
	}
	CTimeRange(CTimeSec tsStartDay,CTimeSec tsEndDay){

		_tsStartDay = tsStartDay;
		_tsEndDay = tsEndDay;

	};
	~CTimeRange(){};

	time_t GetStart(){
		return _tsStartDay.GetTime();
	}

	double GetDiffSecond(){
		return difftime(_tsEndDay.GetTime() ,_tsStartDay.GetTime());
	}

private:
	CTimeSec	_tsStartDay;
	CTimeSec	_tsEndDay;
};
typedef std::vector<CTimeRange>	TIMERANGE_VECTOR;

class CMember {

public:
	CMember(unsigned long dwID,tstring strName){
		_dwMemberID = dwID;
		_strMemberName = strName;
	};

	~CMember(){};

	void AddCapableTask(CTaskKind tkAdd){
		_vecCapable.push_back(tkAdd);
	}

protected:

	unsigned long		_dwMemberID;
	tstring				_strMemberName;	//メンバ名称
	TIMERANGE_VECTOR	_vecHoliday;
	TASKKIND_VECTOR		_vecCapable;	//対応可能タスク

	unsigned char		_byMultiTask;	//並列実行可能なタスク数。0だと単一タスク

	unsigned char		_byPerformance;	//能力　稼働率　100(%)が標準
};
typedef std::vector<CMember>	MEMBER_VECTOR;

typedef enum{

	eTBT_FIXEDDAYS,		//固定の日数をバッファとして持たせる
	eTBT_PERCENTDAYS,	//タスクの日数のX%をバッファとして持たせる

} ENUM_TASKBUFFERTYPE;

class CGeneralConfig {

public:
	CGeneralConfig(){
		for(auto && hd : _abHoliday){
			hd = false;
		}
	};
	~CGeneralConfig(){}

	bool SetPjRange(CTimeRange trPj){
		_trProject = trPj;
		return true;
	}
	CTimeRange GetPjRange(){
		return _trProject;
	}
	time_t GetPjStart(){
		return _trProject.GetStart();
	}

	bool SetHolidayWD(int nWD,bool bHoliday){
		_abHoliday[nWD] = bHoliday;
		return true;
	}

	bool SetHolidayRange(CTimeRange trAdd){
		_vecHoliday.push_back(trAdd);
		return true;
	}

	bool SetBuffer(ENUM_TASKBUFFERTYPE eTBT,unsigned char byBuf){
		_eTaskBufType = eTBT;
		_byTaskBuf = byBuf;
	}

	bool IsWDHoliday(int nWD){
		return _abHoliday[nWD];
	}

	const TIMERANGE_VECTOR* const GetHolidayRange(){
		return &_vecHoliday;
	}

protected:

	CTimeRange			_trProject;		//プロジェクト期間

	bool				_abHoliday[7];	//標準休日(曜日)　0=日～6=月
	TIMERANGE_VECTOR	_vecHoliday;	//標準休日(期間)

	ENUM_TASKBUFFERTYPE	_eTaskBufType;
	unsigned char		_byTaskBuf;

};

typedef enum{

	eTT_NORMAL = 0,		//標準
	eTT_RELATIVE,		//関連タスクが終了してからしか開始できない
	eTT_TARGETDAY,		//日付が指定されている

} ENUM_TASKTYPE;

//タスククラス
class CTask{

public:
	CTask(){}
	CTask(tstring strName,unsigned char byDay,ENUM_TASKTYPE eType = eTT_NORMAL){
		_strTaskName = strName;
		_byDay = byDay;
		_eTaskType = eType;
	}
	~CTask(){}

	ENUM_TASKTYPE	GetTaskType(){	return _eTaskType;}

	CTimeRange		GetTaskTimeRange(){	return _trTargetDay;}

protected:

	tstring						_strTaskName;
	unsigned char				_byDay;			//工数(日数)
	ENUM_TASKTYPE				_eTaskType;

	std::vector<unsigned long>	_vecRelativeTask;	

	CTimeRange					_trTargetDay;

};
typedef std::vector<CTask>	TASK_VECTOR;

class CTaskSpread {

	typedef enum {
		
		eTSD_MEMBER_START	= 0x0000,
		eTSD_MEMBER_END		= 0x7FFF,

		eTSD_OPEN = 0xFFFF,		//空き・予定なし
		eTSD_TASK = 0xFFFE,		//タスクあり
		eTSD_HLDY = 0xFFFD,		//休日
		eTSD_BUFF = 0xFFFC,		//タスクに対するバッファ

	} ENUM_TASKSPREADDEFINE;

public:
	CTaskSpread(){
		_pwSpread = nullptr;
	}
	~CTaskSpread(){
		if(_pwSpread){
			delete[] _pwSpread;
		}
	}

	bool Create(int nDayNum,int nTaskNum){

		if(_pwSpread){
			return false;
		}

		_nDayNum = nDayNum;

		_pwSpread = new unsigned short[nTaskNum * nDayNum];

		return _pwSpread != nullptr;
	
	};
	bool Clear(){};

	bool SetGeneralHoliday(){}
	bool SetMemberHoliday(){}

	bool SetHoliday(int nTask,int nDay){
		return SetCell(nTask,nDay,eTSD_HLDY);
	}

	bool SetCell(int nTask,int nDay,unsigned short wVal){

		if(!_pwSpread){
			return false;
		}

		_pwSpread[(nTask * _nDayNum) + nDay] = wVal;

		return true;

	}

	bool GetCell(int nTask,int nDay,unsigned short & wVal){

		if(!_pwSpread){
			return false;
		}

		wVal = _pwSpread[(nTask * _nDayNum) + nDay];

		return true;
	}

protected:

	unsigned short*	_pwSpread;	//タスク数×プロジェクト日数の二次元配列。スプレッドシートのイメージ
	int				_nDayNum;

};

bool Generate(CTaskSpread & tsResult,CGeneralConfig & gc,TASK_VECTOR & vecTask,MEMBER_VECTOR & vecMember){

	//日数を算出して、結果用のバッファを確保
	int nDayNum = gc.GetPjRange().GetDiffSecond() / (60 * 60 * 24);
	tsResult.Create(nDayNum,vecTask.size());

	//標準休日(曜日)をセット
	time_t tmtStart = gc.GetPjStart();
	for(int nDay=0;nDay<nDayNum;nDay++){
	
		struct tm* ptmTarget = localtime(&tmtStart);

		if(gc.IsWDHoliday(ptmTarget->tm_wday)){
			for(int nMember=0;nMember<vecMember.size();nMember++){
				tsResult.SetHoliday(nMember,nDay);
			}
		}

		tmtStart += (60 * 60 * 24);
	}

	//標準休日(期間)をセット
	for(auto trHoliday : *gc.GetHolidayRange()){

		tmtStart = trHoliday.GetStart();
		int nHoliNum = trHoliday.GetDiffSecond() / (60*60*24);
		for(int nDay=0;nDay<nHoliNum;nDay++){
			for(int nMember=0;nMember<vecMember.size();nMember++){
				tsResult.SetHoliday(nMember,nDay);
			}
		}
	}

	//タスクの割り当て
	for(auto task : vecTask){

		//あるタスクを割り当てたい時

		//開始条件があれば、開始日を調整する
		time_t	tmStart;
		if(task.GetTaskType() == eTT_NORMAL){
			tmStart = gc.GetPjStart();
		}
		if(task.GetTaskType() == eTT_RELATIVE){
			//関連タスクの終了日の翌日が開始日
			//TODO
		}
		if(task.GetTaskType() == eTT_TARGETDAY){
			//指定日が開始日
			//TODO
		}

		//開始日から順に、持ちタスクのないメンバを探す


		//持ちタスクのないメンバが、担当できるタスクなら次の処理

		//タスク終了見込み日まで空いているか確認　稼働率・マルチタスク考慮

		//空いていれば割り当て

			//見つけた日から順に、埋めていく
			//休日考慮
			//
			
	
	}

}

//メンバが一人。タスクは複数。期間内に終わる場合
//休日は土日のみ
bool test_1(){

	CGeneralConfig	gc;
	TASK_VECTOR		vecTask;
	MEMBER_VECTOR	vecMember;

	//期間登録
	gc.SetPjRange(CTimeRange(CTimeSec(2019,7,7),CTimeSec(2019,9,7)));

	//休日設定
	gc.SetHolidayWD(0,true);	//日
	gc.SetHolidayWD(1,false);
	gc.SetHolidayWD(2,false);
	gc.SetHolidayWD(3,false);
	gc.SetHolidayWD(4,false);
	gc.SetHolidayWD(5,false);
	gc.SetHolidayWD(6,true);	//土

	gc.SetHolidayRange(CTimeRange(CTimeSec(2019,8,12),CTimeSec(2019,8,16)));	//お盆

	//バッファは固定で1日
	gc.SetBuffer(eTBT_FIXEDDAYS,1);

	CMember mem(1,_T("山田 太郎"));
	vecMember.push_back(mem);

	//1回あたり2日かかるタスク×10
	for(int task=0;task<10;task++){
		CTask task_new(_T("task"),2);
		vecTask.push_back(task_new);
	}


}

int main()
{
    return 0;
}

