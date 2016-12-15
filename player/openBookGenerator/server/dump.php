<?php
 
define(CURSCRIPT,'connect');
 
require './include/common.inc.php';
 
$result = mysqli_query($con,"SELECT moves,result FROM tasks");
while ($row = mysqli_fetch_array($result)) 
{
	echo $row['moves']."\n";
	echo $row['result']."\n";
}

?>