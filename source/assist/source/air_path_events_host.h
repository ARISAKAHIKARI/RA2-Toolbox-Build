#pragma once
static void serviceAirEventFeedback(){
 static const void*mapping=nullptr;static uint32_t epoch=0;static long seen=0;static int previous=0;
 if(!shared)return;const uint32_t current=commandMatch();
 if(mapping!=shared||epoch!=current){mapping=shared;epoch=current;seen=0;previous=0;}
 long seq=shared->airEventSeq;if((seq&1)||seq==seen)return;MemoryBarrier();auto r=shared->airEventReport;MemoryBarrier();
 if(seq!=shared->airEventSeq||r.match!=current)return;seen=seq;
 std::wstring text;
 if(r.status==airevents::Recorded)text=L"飞机路径点已记录："+std::to_wstring(r.points)+L" 个；松开游戏路径点键提交。";
 else if(r.status==airevents::Sending&&previous!=airevents::Sending)text=L"正在添加飞机原生路径节点；队列繁忙时自动接续。";
 else if(r.status==airevents::Committed)text=L"飞机路径已交原生松键提交："+std::to_wstring(r.points)+L" 个路径点、"+std::to_wstring(r.accepted)+L" 条节点命令。";
 else if(r.status==airevents::Partial)text=L"飞机路径设置已中断；已入队的节点仍由游戏处理，未提交的部分已停止。";
 else if(r.status==airevents::Full)text=L"飞机路径点已达 64 个，请先松开路径点键提交。";
 else if(r.status<0)text=L"飞机路径点未继续：选择、按键或对局状态已改变，请重新操作。";
 previous=r.status;if(!text.empty())notify(text);
}
