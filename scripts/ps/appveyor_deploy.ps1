###########################################
# Script to deploy OpenRCT2 from AppVeyor #
###########################################

$nottesting = (${env:Configuration} -notlike "*tests")
# Only deploy from VS2017 for now.
$notvs2015 = (${env:APPVEYOR_JOB_NAME} -notlike "*2015*")
if ($nottesting -and $notvs2015)
{
    # Check if OpenRCT2.org API security token is available
    if (${env:OPENRCT2_ORG_TOKEN})
    {
        # Only upload tagged builds, develop branch or push/ branches
        if (${env:APPVEYOR_REPO_TAG} -eq "true" -or ${env:APPVEYOR_REPO_BRANCH} -match "^develop$|^push/")
        {
            msbuild openrct2.proj /t:UploadArtifacts /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"
            curl --data-binary @bin/openrct2-dll.pdb 'https://openrct2.sp.backtrace.io:6098/post?format=symbols&token=f9c5e640d498f15dbe902eab3e822e472af9270d5b0cbdc269cee65a926bf306&upload_file=openrct2-dll.pdb'
            curl --data-binary @bin/openrct2-win.pdb 'https://openrct2.sp.backtrace.io:6098/post?format=symbols&token=f9c5e640d498f15dbe902eab3e822e472af9270d5b0cbdc269cee65a926bf306&upload_file=openrct2-win.pdb'
            curl --data-binary @bin/openrct2.dll 'https://openrct2.sp.backtrace.io:6098/post?format=symbols&token=f9c5e640d498f15dbe902eab3e822e472af9270d5b0cbdc269cee65a926bf306&upload_file=openrct2.dll'
            curl --data-binary @bin/openrct2.exe 'https://openrct2.sp.backtrace.io:6098/post?format=symbols&token=f9c5e640d498f15dbe902eab3e822e472af9270d5b0cbdc269cee65a926bf306&upload_file=openrct2.exe'
        }
        else
        {
            Write-Host "No deployment: Non-tagged or push branch." -ForegroundColor Yellow
        }
    }
    else
    {
        Write-Host "No deployment: %OPENRCT2_ORG_TOKEN% not available." -ForegroundColor Yellow
    }
}
