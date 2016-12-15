<?php

if(!defined('IN_ONLINE_JUDGE')) {
	exit('Access Denied');
}

//----------------------------------------
//              底层机制函数
//----------------------------------------

function gameerrorhandler($code, $msg, $file, $line){
	global $errorinfo;
	if(!$errorinfo){return;}
	if($code == 2){$emessage = '<b style="color:#ff0">Warning</b> ';}
	elseif($code == 4){$emessage = '<b style="color:#f00">Parse</b> ';}
	elseif($code == 8){$emessage = '<b>Notice</b> ';}
	elseif($code == 256){$emessage = '<b>User Error</b> ';}
	elseif($code == 512){$emessage = '<b>User Warning</b> ';}
	elseif($code == 1024){$emessage = '<b>User Notice</b> ';}
	else{$emessage = '<b style="color:#f00>Fatal error</b> ';}
	$emessage .= "($code): $msg in $file on line $line";
	if(isset($GLOBALS['error'])){
		$GLOBALS['error'] .= '<br>'.$emessage;
	}else{
		$GLOBALS['error'] = $emessage;
	}
	return true;
}

function gexit($message = '',$file = '', $line = 0) {
	global $charset,$title,$extrahead,$allowcsscache,$errorinfo;
	include template('error');
	exit();
}

//----------------------------------------
//              输入输出函数
//----------------------------------------

function gstrfilter($str) {
	if(is_array($str)) {
		foreach($str as $key => $val) {
			$str[$key] = gstrfilter($val);
		}
	} else {		
		if($GLOBALS['magic_quotes_gpc']) {
			$str = stripslashes($str);
		}
		$str = str_replace("'","",$str);//屏蔽单引号'
		$str = str_replace("\\","",$str);//屏蔽反斜杠/
		$str = htmlspecialchars($str,ENT_COMPAT);//转义html特殊字符，即"<>&
	}
	return $str;
}

function language($file, $templateid = 0, $tpldir = '') {
	$tpldir = $tpldir ? $tpldir : TPLDIR;
	$templateid = $templateid ? $templateid : TEMPLATEID;

	$languagepack = OJ_ROOT.'./'.$tpldir.'/'.$file.'.lang.php';
	if(file_exists($languagepack)) {
		return $languagepack;
	} elseif($templateid != 1 && $tpldir != './templates/default') {
		return language($file, 1, './templates/default');
	} else {
		return FALSE;
	}
}

function template($file, $templateid = 0, $tpldir = '') {
	global $tplrefresh;
	$tpldir = TPLDIR;
	$templateid = TEMPLATEID;

	$tplfile = OJ_ROOT.'./'.$tpldir.'/'.$file.'.htm';
	$objfile = OJ_ROOT.'./data/templates/'.$templateid.'_'.$file.'.tpl.php';
	if(TEMPLATEID != 1 && $templateid != 1 && !file_exists($tplfile)) {
		return template($file, 1, './templates/default/');
	}
	if($tplrefresh == 1) {
		if(!file_exists($objfile) || filemtime($tplfile) > filemtime($objfile)) {
			require_once OJ_ROOT.'./include/template.func.php';
			parse_template($file, $templateid, $tpldir);
		}
	}
	return $objfile;
}

function config($file = '') {
	$cfgfile = OJ_ROOT."./data/cache/{$file}.data.php";
	return $cfgfile;
}

//读取文件
function readover($filename,$method="rb"){
	strpos($filename,'..')!==false && exit('Forbidden');
	//$filedata=file_get_contents($filename);
	$handle=fopen($filename,$method);
	if(flock($handle,LOCK_SH)){
		$filedata='';
		while (!feof($handle)) {
   		$filedata .= fread($handle, 8192);
		}
		//$filedata.=fread($handle,filesize($filename));
		fclose($handle);
	} else {exit ('Read file error.');}
	return $filedata;
}

//写入文件
function writeover($filename,$data,$method="rb+",$iflock=1,$check=1,$chmod=1){
	$check && strpos($filename,'..')!==false && exit('Forbidden');
	touch($filename);
	$handle=fopen($filename,$method);
	if($iflock){
		if(flock($handle,LOCK_EX)){
			fwrite($handle,$data);
			if($method=="rb+") ftruncate($handle,strlen($data));
			fclose($handle); 
		} else {var_dump($filename);exit ('Write file error.');}
	} else {
		fwrite($handle,$data);
		if($method=="rb+") ftruncate($handle,strlen($data));
		fclose($handle); 
	}
	$chmod && chmod($filename,0777);
	return;
}

//打开文件，以数组形式返回
function openfile($filename){
	$filedata=readover($filename);
	$filedata=str_replace("\n","\n<:game:>",$filedata);
	$filedb=explode("<:game:>",$filedata);
	$count=count($filedb);
	if($filedb[$count-1]==''||$filedb[$count-1]=="\r"){unset($filedb[$count-1]);}
	if(empty($filedb)){$filedb[0]='';}
	return $filedb;
}

function gsetcookie($var, $value, $life = 0, $prefix = 1) {
	global $tablepre, $gtablepre, $cookiedomain, $cookiepath, $now, $_SERVER;
	setcookie(($prefix ? $gtablepre : '').$var, $value,
		$life ? $now + $life : 0, $cookiepath,
		$cookiedomain, $_SERVER['SERVER_PORT'] == 443 ? 1 : 0);
}

function clearcookies() {
	global $cookiepath, $cookiedomain, $game_uid, $game_user, $game_pw, $game_secques, $adminid, $groupid, $credits;
	dsetcookie('auth', '', -86400 * 365);

	$game_uid = $adminid = $credits = 0;
	$game_user = $game_pw = $game_secques = '';
}

function check_alnumudline($key)
{
	$key=(string)$key;
	for ($i=0; $i<strlen($key); $i++)
	{
		if (!(('a'<=$key[$i] && $key[$i]<='z') || ('A'<=$key[$i] && $key[$i]<='Z') || $key[$i]=='_' || ('0'<=$key[$i] && $key[$i]<='9')))
			return false;
	}
	return true;
}

?>
