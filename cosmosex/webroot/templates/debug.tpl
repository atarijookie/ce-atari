<b>CosmosEx App version: </b>{{version_app}}<br />
<b>HTTP Headers:</b>{{browser_headers}}
<b>CosmosEx date (DD.MM.YYYY): </b>{{date}}<br />
<br /><br />
Download <a href="/app/debug/getlog">app log</a>.
<br /><br />
<label for="loglevel">Set log level</label>
<select name="loglevel" id="setloglevel">
<option value="">none</option>
<option value="ll1">ll1</option>
<option value="ll2">ll2</option>
<option value="ll3">ll3</option>
</select>
<br /><br />
<script language="JavaScript">
$(document).ready(function(){
  $('#setloglevel').on('change', function() {
    var sLogLevel=$(this).val();
    $.ajax({
        url : "/api/v1/config/loglevel",
        type: "POST",
        data : {"loglevel":sLogLevel},
        success: function(data, textStatus, jqXHR)
        {
        },
        error: function (jqXHR, textStatus, errorThrown)
        {
        }
    });    
  });
});
</script>
