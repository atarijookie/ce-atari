/*================================================================
  CosmosEx floppy manager
  ================================================================
  Just a quick usage example for the CosmosEx HTTP Floppy API
  ----------------------------------------------------------------
  RESTlike interface, using json

  /api/v1/floppy/[slot# 0-2] 	POST
  Upload file
  
  /api/v1/floppy/[slot# 0-2] 	PUT
  Activate slot - all others are deactivated

  api/v1/floppy/ 	GET
	{"slots":["filename_slot1.msa","filename_slot2.msa","filename_slot3.msa"]}

  ----------------------------------------------------------------*/
//TODO:
  
var CosmosEx = CosmosEx || {};

CosmosEx.Floppy=function(){
  var refreshFilenames=function(){
  	$.ajax({
  		type: 'GET',
  		url: '/api/v1/floppy',
      success: function(data){
        for( var i=0; i<3; i++){
          $("input[type=checkbox][data-slot='"+i+"']").prop('checked', false);
          if( typeof data.slots[i]!="undefined" ){
            var file=data.slots[i];
            $("span.file-"+i).removeClass("empty");
            $("div[data-slot='"+i+"'] label.checkbox").removeClass("empty");
            if( file=='' ){
              file="empty";
              $("span.file-"+i).addClass("empty");
              $("div[data-slot='"+i+"'] label.checkbox").addClass("empty");
            }
            $("span.file-"+i).html(file);
          }
        }  
        if( data.active!=null ){
          $("input[type=checkbox][data-slot='"+data.active+"']").prop('checked', true);
        }
      },
      error: function(){
      }
  	});
  };
  var onFileChange=function(e){
    if(this.files.length==0){
      $(this).siblings(".upload").addClass("vis-hidden");
      return;
    }
    var file=this.files[0];
    var name=file.name;
    var size=file.size;
    var ext=name.replace(/^.*\./, '');
    if( name==ext ){
      $(this).siblings(".upload").addClass("vis-hidden");
      return;
    }
    ext=ext.toLowerCase();
    switch(ext){
      case "msa":
      case "st":
        break;
      default:
        alert("Only file types MSA and ST are allowed.");
        $(this).siblings(".upload").addClass("vis-hidden");
        return;
        break;
    }
    $(this).siblings(".upload").removeClass("vis-hidden");
  };
  var onActivate=function(e){
    var slotid=$(this).data("slot");
    if(!$(this).is(":checked")){
        slotid=255;
    }

    $.ajax({
        url: '/api/v1/floppy/'+slotid,  //Server script to process data
        type: 'PUT',
        success: function(){
            refreshFilenames();
        },
        error: function(){
        },
        //Options to tell jQuery not to process data or worry about content-type.
        cache: false,
        contentType: false,
        processData: false
    });    
  }
  var onSubmit=function(e){
    var $form=$(this).parent("form");
    var $progress=$form.siblings("progress");
    var $filename=$form.siblings(".file.name");
    var $button=$(this);
    var slotid=$(this).data("slot");
    var formData = new FormData($form[0]);
    function progressHandlingFunction(e){
      if(e.lengthComputable){
        $progress.attr({value:e.loaded,max:e.total});
      }
    }; 
    $.ajax({
        url: '/api/v1/floppy/'+slotid,  //Server script to process data
        type: 'POST',
        xhr: function() {  // Custom XMLHttpRequest
            var myXhr = $.ajaxSettings.xhr();
            if(myXhr.upload){ // Check if upload property exists
                myXhr.upload.addEventListener('progress',progressHandlingFunction, false); // For handling the progress of the upload
            }
            return myXhr;
        },
        //Ajax events
        beforeSend: function(){
          $progress.show();
          $filename.hide();  
        },
        success: function(){
          $progress.hide();
          $button.addClass("vis-hidden");  
          refreshFilenames();  
          $filename.show();  
        },
        error: function(){
          $progress.hide();  
          $button.addClass("vis-hidden");  
          refreshFilenames();  
          $filename.show();
        },
        // Form data
        data: formData,
        //Options to tell jQuery not to process data or worry about content-type.
        cache: false,
        contentType: false,
        processData: false
    });    
    e.preventDefault();
    return false;
  };
  return {
    init:function(){
        refreshFilenames();
        $("input[type='submit']").click(onSubmit);
        $('.input-file').change(onFileChange);
        $("input[type='checkbox']").change(onActivate);
      } 
  };
}();