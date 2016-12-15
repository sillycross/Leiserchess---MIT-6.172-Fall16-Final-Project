<?php

define(CURSCRIPT,'connect');

require './include/common.inc.php';

if (!isset($_GET['password'])) die();
if ($_GET['password'] !== '6172finaLpassw0rd') die();
	
if (!isset($_GET['report']))
{
	$plock=fopen(OJ_ROOT.'./data/process.lock','ab');
	flock($plock,LOCK_EX);
	$arr=Array(0=>11,1=>10,2=>9);
	for ($j=0; $j<3; $j++)
	{
		$i=$arr[$j];
		$result = mysqli_query($con,"SELECT COUNT(*) FROM tasks WHERE depth='$i' AND status='0'");
		while ($row = mysqli_fetch_array($result)) 
		{
			$t=$row[0];
		}
		if ($t==0) continue;
		
		$result = mysqli_query($con,"SELECT * FROM tasks WHERE depth='$i' AND status='0' ORDER BY len LIMIT 1");
		while ($row = mysqli_fetch_array($result)) 
		{
			$gid=$row['gid'];
			$depth=$row['depth'];
			$moves=$row['moves'];
			echo $gid."\n";
			echo 'position startpos moves '.$moves."\n";
			echo 'go depth '.$depth."\n"; 
			echo 'quit'."\n";
		}
		mysqli_query($con,"UPDATE tasks SET status='1' WHERE gid='$gid'");
		fclose($plock); 
		die();
	}
	echo "0\n";
	fclose($plock); 
}
else
{
	if (!isset($_GET['gid'])) die();
	$gid=(int)$_GET['gid'];
	if (!isset($_GET['result'])) die();
	$result=$_GET['result'];
	if (!check_alnumudline($result)) die();
	
	$plock=fopen(OJ_ROOT.'./data/process.lock','ab');
	flock($plock,LOCK_EX);
	mysqli_query($con,"UPDATE tasks SET result='$result' WHERE gid='$gid'");
	mysqli_query($con,"UPDATE tasks SET status='2' WHERE gid='$gid'");
	fclose($plock); 
}

?>