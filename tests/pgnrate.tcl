#!/usr/bin/tclsh

set tmpfile     tmp.pgn
set anchor      nobody
set elo         0


# notes on above parameters:
# --------------------------
# tmpfile -  tcl doesn't have a way to create a tmpfile without 
#            loading a library.
#
# anchor -   not required, but ratings average zero otherwise.
#
# prior -    strength of assumption that all players are equal.
#            Remi says it should not be zero.
#            I think default is 2 or 3
#
# drawelo -  Should not be zero.  Reflects the ELO advantage 
#            of having the white pieces in chess.



proc usage {} {
    puts stderr {
        usage: rate.tcl [OPTION] ... [FILE] ...

        -anchor   PLAYER
                  specify name of a player to assign fixed ELO rating

        -elo      RATING
                  specify rating of anchor player

        rate.tcl assumes the external bayeselo program is in current directory
    }
}


if { $argc == 0 } {
    usage
    exit 1
} else {

    # allowable states
    # ----------------
    array set valid_states { 
        "anchor" 1
        "elo" 1
        "prior" 1
        "drawelo" 1
    }

    set state ""
    set files {}
    
    foreach p $argv {

        if { [string index $p 0] == "-" } {
            set state [string range $p 1 end]
            if { ![info exists valid_states($state)] } {
                puts stderr "\n  \"$state\" not a valid command line argument"
                usage
                exit 1
            }
        } else {
            if { $state == "" } {
		set tmpfile $p
            } else {
                set $state $p
                set state {}
            }
        }
    }
}

puts stderr ""
puts stderr "  anchor: $anchor"
puts stderr "     elo: $elo"

# set up empty lists for white, black and res
# -------------------------------------------
set w {}
set b {}
set r {}


# parse every SGF file
# --------------------

foreach f $files {

    set pw {}
    set pb {}
    set re {}

    puts -nonewline stderr "\nFile: $f  .... "
    set h [open $f]

    set dta [read $h]

    foreach {m n} [regexp -all -inline -- {PW\[(.*?)\]} $dta] {
        lappend pw $n
    }

    foreach {m n} [regexp -all -inline -- {PB\[(.*?)\]} $dta] {
        lappend pb $n
    }

    foreach {m n} [regexp -all -inline -- {RE\[(.*?)\]} $dta] {
        lappend re $n
    }

    set len [llength $pw]
    
    if { [llength $pb] != $len || [llength $re] != $len } {
        puts stderr "Sorry - cannot parse $f"
        continue
    }

    set w [concat $w $pw]
    set b [concat $b $pb]
    set r [concat $r $re]

    puts stderr "Ok.  Found [llength $pw] games."

    close $h
}



set pip [open "|../BayesElo/bayeselo" r+]
fconfigure $pip -buffering line

puts $pip "readpgn $tmpfile"
#puts $pip "bayeselo"
#puts $pip "elostat"
puts $pip "elo"

#puts $pip "prior 1"
puts $pip "drawelo 10"
#puts $pip "confidence 0.98"
puts $pip "mm"
puts $pip "exactdist"
puts $pip "ratings"
#puts $pip "offset $elo $anchor"
#puts $pip ""
# puts $pip "los"
puts $pip "x"
puts $pip "x"

set ok 0
set perc ""

puts ""

while { [gets $pip s] >= 0 } {

    if { $ok == 0 } {
        if { [regexp {(Rank\s+Name\s+.*)} $s dmy h1] } {
            puts $h1
            set ok 1
            continue
        }
    }

    if { $ok == 1 } {
	if { ![regexp {^ResultSet-EloRating>} $s] } {
	    puts $s
	} else { 
	    break
	}
    }
}

puts ""


set largest 0
set matrix {}
while { [gets $pip s] >= 0 } {
    if { [regexp {^ResultSet-EloRating} $s] } { continue }
    lappend matrix $s
    set sl [string length $s]
    if { $sl > $largest } { set largest $sl }
}

set count [llength $matrix]

set f1 [expr $largest - $count * 3]

if { 0 } {
    puts "     Likelihood Of Superiority Matrix"
    puts ""
    
    set space {                                                                }
    
    # puts -nonewline [string range $space 0 [expr $f1 + 5]]
    puts -nonewline "      "
    for {set i 1} {$i <= $count} {incr i} {
        puts -nonewline [format "%4d" $i]
    }
    puts ""
    
    puts -nonewline "      "
    # puts -nonewline [string range $space 0 [expr $f1 + 5]]
    for {set i 1} {$i <= $count} {incr i} {
        puts -nonewline " ---"
    }
    puts ""

    set c 0
    foreach s $matrix {
        set n [string range $s 0 [expr $f1 - 1]]
        incr c
        # puts -nonewline [format "%4d $n  " $c]
        puts -nonewline [format "%4d   " $c $n]
        for {set i 0} {$i < $count} {incr i} {
            set v [string range $s [expr $f1 + $i * 3] [expr $f1 + $i * 3 + 2]]
            puts -nonewline [format "%-4s" $v]
        }
        puts ""
    }
    
    puts ""
}
