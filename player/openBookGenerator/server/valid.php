<?php

define(CURSCRIPT,'valid');

require './include/common.inc.php';
 
if (!isset($_POST['pass']) || $_POST['pass']!=='orzxyz')
{
	gexit('Wrong password.');
	die();
}

gsetcookie('pass',$_POST['pass']);
header('Location: index.php');

?>
