<?php if(!defined('IN_ONLINE_JUDGE')) exit('Access Denied'); include template('header'); ?>
<center>
<div style="width:80%; text-align:left;">
This website is password-protected. Enter the correct password to continue.<br><br>
<form action="valid.php" method="post">
<input type="password" name="pass" value=""><br>
<input type="submit" value="Submit"/>
</form>
</div>
</center>
