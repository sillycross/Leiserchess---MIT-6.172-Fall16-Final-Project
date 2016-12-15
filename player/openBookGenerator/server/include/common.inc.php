<?php

define('IN_ONLINE_JUDGE', TRUE);
define('OJ_ROOT', substr(dirname(__FILE__), 0, -7));
define('STYLEID', '1');
define('TEMPLATEID', '1');
define('TPLDIR', './templates/default');

if(PHP_VERSION < '4.3.0') {
	exit('PHP version must >= 4.3.0!');
}
require OJ_ROOT.'./include/global.func.php';
require OJ_ROOT.'./config.inc.php';

error_reporting(E_ALL);
set_error_handler('gameerrorhandler');

date_default_timezone_set('Etc/GMT');
$now = time() + $moveut*3600 + $moveutmin*60;   
list($sec,$min,$hour,$day,$month,$year,$wday) = explode(',',date("s,i,H,j,n,Y,w",$now));

if (!defined('NO_MYSQL_CONNECT'))
{
	$con = mysqli_connect($dbhost,$dbuser,$dbpasswd);
	if (!$con)
	{
		die('Could not connect to mysql server: ' . mysqli_error());
	}
	mysqli_select_db($con,$dbname);
}

if (!isset($_COOKIE['pass']) && CURSCRIPT !== 'valid' && CURSCRIPT !== 'connect')
{
	include template('valid');
	die();
}

?>
