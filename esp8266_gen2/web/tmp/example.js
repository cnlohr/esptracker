//Copyright (C) 2016 <>< Charles Lohr, see LICENSE file for more info.
//
//This particular file may be licensed under the MIT/x11, New BSD or ColorChord Licenses.

function initLighthouse() {
var menItm = `
	<tr><td width=1><input type=submit onclick="ShowHideEvent( 'Lighthouse' );" value="Lighthouse"></td><td>
	<div id=Lighthouse class="collapsible">
		<input type=button id=InfoBtn value="Pull Frame">
		<div style="overflow-x: scroll; width: 800px">
			<div id="packetstatus"></div>
			<canvas id="packetcanvas" width=800 height=100></canvas>
		</div>
		<textarea id="packetraw" rows="1" cols="20" wrap="off"></textarea>
    </div>
	</td></tr>
`;
	$('#MainMenu > tbody:last').after( menItm );

	$('#InfoBtn').click( function(e) {
		$('#InfoBtn').val('Getting data...');
		$('#InfoDspl').html('&nbsp;');
		QueueOperation( "CE", clbInfoBtn ); // Send info request to ESP
	});
}

window.addEventListener("load", initLighthouse, false);


var vtext = "";

function popParam( dataarr )
{
	var i = 0;
	for( i = 0; i < dataarr[0].length; i++ )
	{
		var c = dataarr[0].charAt(i);
		if( c == '\t' ) break;
	}
	var popped = dataarr[0].substr(0,i);
	dataarr[0] = dataarr[0].substr( i+1 );
	return popped;
}

function clbInfoBtn(req,data,rawdat) {
	$('#InfoBtn').val('Re-get');
	var dataarr = [data];

	var cmd = popParam( dataarr );
	var State = popParam( dataarr );
	var Size = popParam( dataarr );
	console.log( cmd + " " + State + " " + Size );

	var Edges = [];

	vtext = "";
	var rawdat = dataarr[0];
	var firstedge = 0;

	for( var i = 0; i < 16; i++ )
	{
		var n = rawdat.charCodeAt(i).toString(16);
		if( n.length == 1 ) n = "0" + n;
		vtext += " " + n;
	}
	vtext += "\n";

	var valid = rawdat.charCodeAt(0);
	var freq_denom = rawdat.charCodeAt(1);
	var freq_num = rawdat.charCodeAt(3)<<8 | rawdat.charCodeAt(2);
	var full_length = rawdat.charCodeAt(5)<<8 | rawdat.charCodeAt(4);
	var strength = rawdat.charCodeAt(7)<<8 | rawdat.charCodeAt(6);
	var average_numerator = rawdat.charCodeAt(11)<<24 | rawdat.charCodeAt(10)<<16 | rawdat.charCodeAt(9)<<8 | rawdat.charCodeAt(8);
	var firsttransition = rawdat.charCodeAt(15)<<24 | rawdat.charCodeAt(14)<<16 | rawdat.charCodeAt(13)<<8 | rawdat.charCodeAt(12);

	vtext += "ID: " + valid + "\n";
	vtext += "At: " + firsttransition + "\n";
	vtext += "Length: " + full_length + "\n";
	vtext += "Center: " + average_numerator * 1.0 / strength + "\n";
	vtext += "Freq: " + 2.0 * 40000000.0 * freq_denom / freq_num + "\n";
	vtext += "Strength: " + strength + "\n";

	for (var i = 16; i < rawdat.length;﻿ i+=4)
	{
		var d = rawdat.charCodeAt(i+0)<<24;
		d |= rawdat.charCodeAt(i+1)<<16;
		d |= rawdat.charCodeAt(i+2)<<8;
		d |= rawdat.charCodeAt(i+3)<<0;
		Edges.push( d )
		vtext += d + "\n";
	}

	$("#packetraw").text( vtext );

	QueueOperation( "CP", callbackCP ); // Send info request to ESP
}

// Handle request previously sent on button click
function callbackCP(req,data,rawdat) {
	$('#InfoBtn').val('Display Info');
	var dataarr = [data];

	var cmd = popParam( dataarr );
	var State = popParam( dataarr );
	var Size = popParam( dataarr );

	if( State == 2 )
	{
		var Data = [];
		var rawdat = dataarr[0];
		for (var i = 0; i < rawdat.length;﻿ i++)
		{
			var d = rawdat.charCodeAt(i);
			Data.push( (d & 128) != 0 );
			Data.push( (d & 64) != 0 );
			Data.push( (d & 32) != 0 );
			Data.push( (d & 16) != 0 );
			Data.push( (d & 8) != 0 );
			Data.push( (d & 4) != 0 );
			Data.push( (d & 2) != 0 );
			Data.push( (d & 1) != 0 );
		}
		$('#packetstatus').html( "Words: " + Size );
		var canvwid = Data.length; //$("#packetcanvas").width();
		var canvhei = 100;
//( canvwid, canvhei );
		var ctx = $("#packetcanvas")[0].getContext("2d");
		ctx.height = canvhei;
		ctx.width = canvwid;
		$("#packetcanvas")[0].width = canvwid;

/* To allow overlaying, do this code.
		if( ctx.width != 6000 )
		{
			ctx.width = 6000;
			$("#packetcanvas")[0].width = 6000;
		}
*/
		var bypp = Data.length / canvwid;
		ctx.beginPath();
		ctx.moveTo(0,0);
		var i = 0;
		for( var x = 0; i < Data.length; x+=bypp )
		{
			var y = Data[i++]?10:canvhei-50;
			ctx.lineTo(x, y);
		}
		ctx.stroke();
/*
		for( i = 0; i < Data.length; i++ )
		{
			vtext += Data[i]?'1':'0';
		}
		vtext+="\n";
		$("#packetraw").text( vtext );
*/
	}
}
