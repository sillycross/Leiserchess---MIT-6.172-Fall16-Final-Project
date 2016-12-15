<?php

require './include/common.inc.php';

for ($i=9; $i<=11; $i++)
{
	$result = mysqli_query($con,"SELECT COUNT(*) FROM tasks WHERE depth='$i'");
	while ($row = mysqli_fetch_array($result)) 
	{
		$t[$i][0]=$row[0];
	}
	$result = mysqli_query($con,"SELECT COUNT(*) FROM tasks WHERE depth='$i' AND status='1'");
	while ($row = mysqli_fetch_array($result)) 
	{
		$t[$i][1]=$row[0];
	}
	$result = mysqli_query($con,"SELECT COUNT(*) FROM tasks WHERE depth='$i' AND status='2'");
	while ($row = mysqli_fetch_array($result)) 
	{
		$t[$i][2]=$row[0];
	}
}

include template('index');

?>

