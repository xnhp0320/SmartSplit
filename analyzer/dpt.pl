#!/usr/bin/perl

use warnings;
use strict;

open my $f, "$ARGV[0]" or die "error:$!";

my $node;
my %Tree_L;
my %Tree_R;

my %cnt;

while(<$f>){

    if(/^node (\d+)$/) {
        $node = "$1";
        $Tree_L{$node} = [ ];
        $Tree_R{$node} = [ ];
    }
    if(/^\s+lchild (\d+)/) {
        #print "$1";
        push @{$Tree_L{$node}}, $1;
    }

    if(/^\s+rchild (\d+)/) {
        push @{$Tree_R{$node}}, $1; 
    }

    if(/^\s+cnt (\d+)/) {
        $cnt{$node} = $1; 
    }
}
#print %Tree;

close $f;

open OUT, ">", "graph-tmp"."$ARGV[0]" or die "error:$!";

print OUT "digraph A{\n";

for my $key (sort {$a<=>$b} keys %cnt)
{
    print OUT "$key [label = \"$cnt{$key}\" ];\n";
}


for my $key (sort {$a<=>$b} keys %Tree_L)
{
    #print $key;
    #print "@{$Tree{$key}}";
    for my $c (@{$Tree_L{$key}}){
        print OUT "$key -> $c [label = \"0\" ];\n";
    }
}

for my $key (sort {$a<=>$b} keys %Tree_R)
{
    #print $key;
    #print "@{$Tree{$key}}";
    for my $c (@{$Tree_R{$key}}){
        print OUT "$key -> $c [label = \"1\" ];\n";
    }
}

print OUT  "}";

close OUT;


`dot -Tpng graph-tmp$ARGV[0] -o $ARGV[0].png`;


#my @a=(1,2,2);
#print $#a;







