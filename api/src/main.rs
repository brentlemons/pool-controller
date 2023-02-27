use lambda_http::{run, service_fn, Body, Error, Request, Response};
use aws_config::meta::region::RegionProviderChain;
use aws_sdk_iotdataplane::{Region};
use structopt::StructOpt;
use aws_smithy_types::{Blob};
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct GetEndpointRejected<'a> {
    pub code: u16,
    pub message: &'a str,
}

#[derive(Debug, StructOpt)]
struct Opt {
    /// The AWS Region.
    #[structopt(short, long)]
    region: Option<String>,

    /// Whether to display additional information.
    #[structopt(short, long)]
    verbose: bool,
}

/// This is the main body for the function.
/// Write your code inside it.
/// There are some code examples in the Runtime repository:
/// - https://github.com/awslabs/aws-lambda-rust-runtime/tree/main/examples
async fn function_handler(_event: Request) -> Result<Response<Body>, Error> {
    // Extract some useful information from the request
    let Opt { region, verbose } = Opt::from_args();

    let region_provider = RegionProviderChain::first_try(region.map(Region::new))
        .or_default_provider()
        .or_else(Region::new("us-east-1"));

    let shared_config = aws_config::from_env().region(region_provider).load().await;
    let client = aws_sdk_iotdataplane::Client::new(&shared_config);
    
    client
        .publish()
        .topic("esp32/sub")
        .qos(1)
        .payload(Blob::new(serde_json::to_vec(&GetEndpointRejected {
            code: 401,
            message: &format!("hello world"),
        })?))
        .send()
        .await
        .unwrap();

    // Return something that implements IntoResponse.
    // It will be serialized to the right response event automatically by the runtime
    let resp = Response::builder()
        .status(200)
        .header("content-type", "text/html")
        .body("Hello AWS Lambda HTTP request".into())
        .map_err(Box::new)?;
    Ok(resp)
}

#[tokio::main]
async fn main() -> Result<(), Error> {
    tracing_subscriber::fmt()
        .with_max_level(tracing::Level::INFO)
        // disabling time is handy because CloudWatch will add the ingestion time.
        .without_time()
        .init();

    run(service_fn(function_handler)).await
}
