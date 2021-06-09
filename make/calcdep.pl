#!/usr/bin/env perl
#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
#   Copyright (C) 2013, 2015-2019, 2021 Sadie Powell <sadie@witchery.services>
#   Copyright (C) 2012 Robby <robby@chatbelgie.be>
#   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
#
# This file is part of InspIRCd.  InspIRCd is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, version 2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


use strict;
use warnings FATAL => qw(all);

use File::Basename qw(basename dirname);
use FindBin        qw($RealDir);

use lib dirname $RealDir;
use make::common;

use constant {
	BUILDPATH  => $ENV{BUILDPATH},
	SOURCEPATH => $ENV{SOURCEPATH},
	DLL_EXT    => $^O eq 'darwin' ? '.dylib' : '.so'
};

sub find_output;
sub gendep($);
sub dep_cpp($$$);
sub dep_so($);
sub dep_dir($$);
sub run();

my %f2dep;

run;
exit 0;

sub run() {
	create_directory(BUILDPATH, 0770) or die "Could not create build directory: $!";
	chdir BUILDPATH or die "Could not open build directory: $!";
	mkdir $_ for qw/bin modules obj/;

	open MAKE, '>real.mk' or die "Could not write real.mk: $!";
	chdir "${\SOURCEPATH}/src";

	run_dynamic();
	close MAKE;
}

sub run_dynamic() {
	print MAKE <<END;
# DO NOT EDIT THIS FILE
# It is autogenerated by make/calcdep.pl, and will be overwritten
# every time you rerun make in the main directory
VPATH = \$(SOURCEPATH)/src

bad-target:
	\@echo "This Makefile must be run by a sub-make from the source"
	\@echo "in order to set the correct environment variables"
	\@exit 1

all: inspircd modules

END
	my(@core_deps, @modlist);
	for my $file (<*.cpp>, "socketengines/$ENV{SOCKETENGINE}.cpp") {
		my $out = find_output $file;
		dep_cpp $file, $out, 'gen-o';
		# Having a module in the src directory is a bad idea because it will be linked to the core binary
		if ($file =~ /^(m|core)_.*\.cpp/) {
			my $correctsubdir = ($file =~ /^m_/ ? "modules" : "coremods");
			print "Error: module $file is in the src directory, put it in src/$correctsubdir instead!\n";
			exit 1;
		}
		push @core_deps, $out;
	}

	for my $directory (qw(coremods modules)) {
		opendir(my $moddir, $directory);
		for my $file (sort readdir $moddir) {
			next if $file =~ /^\./;
			if ($directory eq 'modules' && -e "modules/extra/$file" && !-l "modules/$file") {
				# Incorrect symlink?
				print "Replacing symlink for $file found in modules/extra\n";
				rename "modules/$file", "modules/$file~";
				symlink "extra/$file", "modules/$file";
			}
			if ($file =~ /^(?:core|m)_/ && -d "$directory/$file" && dep_dir "$directory/$file", "modules/$file") {
				mkdir "${\BUILDPATH}/obj/$file";
				push @modlist, "modules/$file${\DLL_EXT}";
			}
			if ($file =~ /^.*\.cpp$/) {
				my $out = dep_so "$directory/$file";
				push @modlist, $out;
			}
		}
	}

	my $core_mk = join ' ', @core_deps;
	my $mods = join ' ', @modlist;
	print MAKE <<END;

bin/inspircd: $core_mk
	@\$(SOURCEPATH)/make/unit-cc.pl core-ld \$\@ \$^ \$>

inspircd: bin/inspircd

modules: $mods

.PHONY: all bad-target inspircd modules

END
}

sub find_output {
	my $file = shift;
	my($path,$base) = $file =~ m#^((?:.*/)?)([^/]+)\.cpp# or die "Bad file $file";
	if ($path eq 'modules/' || $path eq 'coremods/') {
		return "modules/$base${\DLL_EXT}";
	} elsif ($path eq '' || $path eq 'modes/' || $path =~ /^[a-z]+engines\/$/) {
		return "obj/$base.o";
	} elsif ($path =~ m#modules/(m_.*)/# || $path =~ m#coremods/(core_.*)/#) {
		return "obj/$1/$base.o";
	} else {
		die "Can't determine output for $file";
	}
}

sub gendep($) {
	my $f = shift;
	my $basedir = $f =~ m#(.*)/# ? $1 : '.';
	return $f2dep{$f} if exists $f2dep{$f};
	$f2dep{$f} = '';
	my %dep;
	my $link = readlink $f;
	if (defined $link) {
		$link = "$basedir/$link" unless $link =~ m#^/#;
		$dep{$link}++;
	}
	open my $in, '<', $f or die "Could not read $f";
	while (<$in>) {
		if (/^\s*#\s*include\s*"([^"]+)"/) {
			my $inc = $1;
			next if $inc eq 'config.h' && $f eq '../include/inspircd.h';
			my $found = 0;
			for my $loc ("$basedir/$inc", "${\SOURCEPATH}/include/$inc", "${\SOURCEPATH}/vendor/$inc") {
				next unless -e $loc;
				$found++;
				$dep{$_}++ for split / /, gendep $loc;
				$loc =~ s#^\.\./##;
				$dep{$loc}++;
			}
			if ($found == 0 && $inc ne 'inspircd_win32wrapper.h') {
				print STDERR "WARNING: could not find header $inc for $f\n";
			} elsif ($found > 1 && $basedir ne "${\SOURCEPATH}/include") {
				print STDERR "WARNING: ambiguous include $inc in $f\n";
			}
		}
	}
	close $in;
	$f2dep{$f} = join ' ', sort keys %dep;
	$f2dep{$f};
}

sub dep_cpp($$$) {
	my($file, $out, $type) = @_;
	gendep $file;

	print MAKE "$out: $file $f2dep{$file}\n";
	print MAKE "\t@\$(SOURCEPATH)/make/unit-cc.pl $type \$\@ \$(SOURCEPATH)/src/$file \$>\n";
}

sub dep_so($) {
	my($file) = @_;
	my $out = find_output $file;

	my $name = basename $out, DLL_EXT;
	print MAKE ".PHONY: $name\n";
	print MAKE "$name: $out\n";

	dep_cpp $file, $out, 'gen-so';
	return $out;
}

sub dep_dir($$) {
	my($dir, $outdir) = @_;
	my @ofiles;
	opendir DIR, $dir;
	for my $file (sort readdir DIR) {
		next unless $file =~ /(.*)\.cpp$/;
		my $ofile = find_output "$dir/$file";
		dep_cpp "$dir/$file", $ofile, 'gen-o';
		push @ofiles, $ofile;
	}
	closedir DIR;
	if (@ofiles) {
		my $ofiles = join ' ', @ofiles;
		my $name = basename $outdir;
		print MAKE ".PHONY: $name\n";
		print MAKE "$name: $outdir${\DLL_EXT}\n";
		print MAKE "$outdir${\DLL_EXT}: $ofiles\n";
		print MAKE "\t@\$(SOURCEPATH)/make/unit-cc.pl link-dir \$\@ ${\SOURCEPATH}/src/$dir \$^ \$>\n";
		return 1;
	} else {
		return 0;
	}
}

