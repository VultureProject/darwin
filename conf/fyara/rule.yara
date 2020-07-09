/*
    This is a default rule to detect Eicar pattern
    go to https://github.com/Yara-Rules/rules to find more rules available
    or make your own rules using documentation at https://yara.readthedocs.io/en/v3.10.0/writingrules.html
*/

rule eicar
{
	meta:
		description = "Rule to detect Eicar pattern"
		author = "Marc Rivero | @seifreed"
		hash1 = "275a021bbfb6489e54d471899f7db9d1663fc695ec2fe2a2c4538aabf651fd0f"

	strings:
		$s1 = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*" fullword ascii

	condition:
		all of them
}