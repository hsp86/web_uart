$(function(){
    // 执行uart命令，通过ajax发送命令并返回内容
    function execute(cmd,fun)
    {
        var method = "get";
        var action = '/execute';
        var search_data = "cmd=" + cmd;
        $.ajax({
            url: action,
            type: method,
            data: search_data,
            success: fun
        });
    }
    $('#execute_button').bind('click',function(event) {
        execute($('#execute_text').val(),function(msg){
            alert(msg);
        });
    });
    $('#temper').bind('click', function(event) {
        execute('2',function(msg){
            $('#temper').val('温度：'+((parseInt(msg)-317)*4 + 130)/10);
        });
    });
    $('#led').bind('click', function(event) {
        execute('A',function(msg){
            if(msg == '1')
            {
                $('#led').val('led闪烁已打开');
            }
            else
            {
                $('#led').val('led闪烁已关闭');
            }
            
        });
    });
    $('#beep').bind('click', function(event) {
        execute('B',function(msg){
            if(msg == '1')
            {
                $('#beep').val('蜂鸣器已打开');
            }
            else
            {
                $('#beep').val('蜂鸣器已关闭');
            }
            
        });
    });
    $('#relay').bind('click', function(event) {
        execute('C',function(msg){
            if(msg == '1')
            {
                $('#relay').val('继电器已打开');
            }
            else
            {
                $('#relay').val('继电器已关闭');
            }
            
        });
    });


})
